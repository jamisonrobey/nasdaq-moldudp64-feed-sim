import pytest
import socket
import subprocess
import time
import struct
from pathlib import Path
from dataclasses import dataclass


# tests


def test_first_packet_valid(server_process, downstream_client, test_config):
    try:
        data, _ = downstream_client.recvfrom(2048)
    except socket.timeout:
        pytest.fail("Did not receive any data from the downstream feed.")

    header = parse_moldudp64_header(data)
    assert header.session == test_config['session_name']

    messages = get_messages_from_packet(data)

    assert len(messages) == header.message_count


def test_downstream_sequence_is_continuous(server_process, downstream_client):
    try:
        packet1_data, _ = downstream_client.recvfrom(2048)
    except socket.timeout:
        pytest.fail("Could not receive the first packet for sequencing test.")

    header1 = parse_moldudp64_header(packet1_data)
    expected_next_seq_num = header1.sequence_num + header1.message_count

    try:
        packet2_data, _ = downstream_client.recvfrom(2048)
    except socket.timeout:
        pytest.fail("Could not receive the second packet for sequencing test.")

    header2 = parse_moldudp64_header(packet2_data)

    assert header2.sequence_num == expected_next_seq_num


def test_retransmission_of_single_packet(server_process, downstream_client, retransmission_client, test_config):

    try:
        packet_data, _ = downstream_client.recvfrom(2048)
    except socket.timeout:
        pytest.fail("Could not get a ground truth packet from downstream.")

    messages = get_messages_from_packet(packet_data)
    if len(messages) < 2:
        pytest.skip("Not enough messages in the first packet to test retransmission.")

    header = parse_moldudp64_header(packet_data)
    target_seq_num = header.sequence_num + 1
    original_message = messages[1]

    request = create_retrans_request(test_config['session_name'], target_seq_num, 1)
    server_address = ('127.0.0.1', test_config['retrans_port'])
    retransmission_client.sendto(request, server_address)

    try:
        response_packet, _ = retransmission_client.recvfrom(2048)
    except socket.timeout:
        pytest.fail("Did not receive a response from the retransmission server.")

    response_header = parse_moldudp64_header(response_packet)
    assert response_header.sequence_num == target_seq_num
    assert response_header.message_count == 1

    retransmitted_messages = get_messages_from_packet(response_packet)
    assert len(retransmitted_messages) == 1

    assert retransmitted_messages[0] == original_message


def test_retransmission_of_packet_range(server_process, downstream_client, retransmission_client, test_config):
    all_messages = []
    start_seq_num = -1

    while len(all_messages) < 5:
        try:
            packet_data, _ = downstream_client.recvfrom(2048)
            messages = get_messages_from_packet(packet_data)

            if not all_messages:
                header = parse_moldudp64_header(packet_data)
                start_seq_num = header.sequence_num

            all_messages.extend(messages)
        except socket.timeout:
            pytest.fail("Could not collect enough ground truth messages from the downstream feed.")

    target_start_seq = start_seq_num + 1 # Request starting from the second message.
    target_count = 3
    original_messages = all_messages[1:4]

    request = create_retrans_request(test_config['session_name'], target_start_seq, target_count)
    server_address = ('127.0.0.1', test_config['retrans_port'])
    retransmission_client.sendto(request, server_address)

    received_messages = []
    try:
        while len(received_messages) < target_count:
            response_packet, _ = retransmission_client.recvfrom(2048)

            response_header = parse_moldudp64_header(response_packet)

            expected_seq_num = target_start_seq + len(received_messages)
            assert response_header.sequence_num == expected_seq_num

            retrans_msgs = get_messages_from_packet(response_packet)
            received_messages.extend(retrans_msgs)

    except socket.timeout:
        pytest.fail(f"Timed out waiting for retransmission. "
                    f"Received {len(received_messages)} of {target_count} expected messages.")

    assert received_messages == original_messages


def test_retransmission_of_future_packet_is_ignored(server_process, retransmission_client, test_config):
    with pytest.raises(socket.timeout):
        future_seq_num = 999999999
        request = create_retrans_request(test_config['session_name'], future_seq_num, 1)
        server_address = ('127.0.0.1', test_config['retrans_port'])

        retransmission_client.sendto(request, server_address)

        retransmission_client.recvfrom(2048)


@pytest.fixture(scope="module")
def test_config(request):
    path_to_server = request.config.getoption("--path-to-server")
    path_to_sample_file = request.config.getoption("--path-to-sample-file")

    if not path_to_server:
        pytest.fail("--path-to-server is required")
    if not path_to_sample_file:
        pytest.fail("--path-to-sample-file is required")

    return {
        'path_to_server': path_to_server,
        'path_to_sample_file': path_to_sample_file,
        'downstream_port': request.config.getoption("--downstream-port"),
        'downstream_mcast_group': request.config.getoption("--downstream-mcast-group"),
        'retrans_port': request.config.getoption("--retrans-port"),
        'session_name': request.config.getoption("--session-name"),
        'replay_speed': request.config.getoption("--replay-speed"),
    }


@pytest.fixture(scope="module")
def server_process(test_config: dict):
    test_file_path = Path(test_config['path_to_sample_file'])
    if not test_file_path.exists():
        raise FileNotFoundError(f"Test data missing: {test_file_path}")

    server_proc = subprocess.Popen([
        test_config['path_to_server'],
        test_config['session_name'],
        str(test_file_path),
        "--downstream-port", str(test_config['downstream_port']),
        "--downstream-mcast-group", test_config['downstream_mcast_group'],
        "--retrans-port", str(test_config['retrans_port']),
        "--loopback",
        "--replay-speed", str(test_config['replay_speed']),
        "--start", "open"
    ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('', test_config['downstream_port']))

    mcast_group = socket.inet_aton(test_config['downstream_mcast_group'])
    mreq = struct.pack('4sL', mcast_group, socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    sock.settimeout(0.5)
    server_ready = False

    for attempt in range(20):
        if server_proc.poll() is not None:
            stdout, stderr = server_proc.communicate()
            raise RuntimeError(f"Server died:\n{stderr.decode()}")
        try:
            data, _ = sock.recvfrom(1)
            server_ready = True
            break
        except socket.timeout:
            time.sleep(0.5)
            continue

    sock.close()

    if not server_ready:
        server_proc.kill()
        raise RuntimeError("Server never sent data")

    yield server_proc

    server_proc.terminate()
    try:
        server_proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        server_proc.kill()
        server_proc.wait()


@pytest.fixture
def downstream_client(test_config: dict):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('', test_config['downstream_port']))

    mcast_group = socket.inet_aton(test_config['downstream_mcast_group'])
    mreq = struct.pack('4sL', mcast_group, socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    sock.settimeout(0.5)

    yield sock

    sock.close()


@pytest.fixture
def retransmission_client(test_config: dict):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', 0))
    sock.settimeout(0.5)

    yield sock

    sock.close()


# helpers


MOLD_HEADER_SIZE = 20
# 10-byte session + BE unsigned long long (64 bit) sequence + BE unsigned short (16 bit)
MOLD_HEADER_PATTERN = ">10sQH"


@dataclass
class MoldUDP64Header:
    session: str
    sequence_num: int
    message_count: int


def parse_moldudp64_header(data: bytes) -> MoldUDP64Header:
    if len(data) < MOLD_HEADER_SIZE:
        raise ValueError("downstream packet too small")

    header = struct.unpack(MOLD_HEADER_PATTERN, data[:MOLD_HEADER_SIZE])

    session = header[0].decode('ascii')
    seq_num = header[1]
    msg_count = header[2]

    return MoldUDP64Header(
        session=session,
        sequence_num=seq_num,
        message_count=msg_count
    )


def get_messages_from_packet(data: bytes) -> list[bytes]:
    LEN_PREFIX_SIZE = 2

    header = parse_moldudp64_header(data)
    messages = []

    payload = data[MOLD_HEADER_SIZE:]
    curr_offset = 0

    for _ in range(header.message_count):
        if curr_offset + 2 > len(payload):
            raise ValueError(f"Payload truncated before message length could be read. curr_offset {curr_offset}, len(payload) {len(payload)}")

        msg_len_bytes = payload[curr_offset:curr_offset + LEN_PREFIX_SIZE]
        msg_len = int.from_bytes(msg_len_bytes, 'big')

        curr_offset += LEN_PREFIX_SIZE

        if curr_offset + msg_len > len(payload):
            raise ValueError("Payload truncated. Message length exceeds remaining payload size.")

        msg = payload[curr_offset:curr_offset + msg_len]
        messages.append(msg)

        curr_offset += msg_len

    if curr_offset != len(payload):
        raise ValueError("Payload size does not match the sum of its message lengths.")

    return messages


def create_retrans_request(session: str, seq_num: int, count: int) -> bytes:
    session_bytes = session.encode('ascii')
    return struct.pack(MOLD_HEADER_PATTERN, session_bytes, seq_num, count)
