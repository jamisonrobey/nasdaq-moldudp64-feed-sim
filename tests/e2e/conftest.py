#!/usr/bin/env python3
def pytest_addoption(parser):
    parser.addoption(
        "--path-to-server",
        action="store",
        default=None,
        help="Path to nasdaq-moldudp64-feed-sim binary"
    )
    parser.addoption(
        "--path-to-sample-file",
        action="store",
        default=None,
        help="Path to ITCH sample data file"
    )
    parser.addoption(
        "--downstream-port",
        action="store",
        type=int,
        default=3400,
        help="Downstream multicast port"
    )
    parser.addoption(
        "--downstream-mcast-group",
        action="store",
        default="239.0.0.1",
        help="Downstream multicast group"
    )
    parser.addoption(
        "--retrans-port",
        action="store",
        type=int,
        default=3500,
        help="Retransmission TCP port"
    )
    parser.addoption(
        "--session-name",
        action="store",
        default="E2ETEST001",
        help="MoldUDP64 session name"
    )
    parser.addoption(
        "--replay-speed",
        action="store",
        type=float,
        default=0.01,
        help="Replay speed multiplier"
    )
