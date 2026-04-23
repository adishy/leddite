import unittest
from unittest.mock import MagicMock, patch
import struct
import asyncio
from leddite_client import LedditeClient

class TestLedditeClient(unittest.TestCase):
    def setUp(self):
        self.client = LedditeClient("localhost", 8765)

    def test_packet_generation_basic(self):
        # Test a simple 2x2 red square at (5,5)
        # Header: v1, flags=2 (show), w=2, h=2, x=5, y=5, rot=0, bright=255
        pixels = [255, 0, 0] * 4
        packet = self.client._create_packet(2, 2, x=5, y=5, pixels=pixels, flags=2)
        
        expected_header = struct.pack('BBBBbbBB', 1, 2, 2, 2, 5, 5, 0, 255)
        self.assertEqual(packet[:8], expected_header)
        self.assertEqual(packet[8:], bytes(pixels))

    def test_clear_packet(self):
        # Clear should be 1x1 black at (0,0) with flag 1 (clear) and 2 (show)
        packet = self.client._create_packet(1, 1, x=0, y=0, pixels=[0,0,0], flags=1|2)
        flags = packet[1]
        self.assertTrue(flags & 0x01)
        self.assertTrue(flags & 0x02)

    @patch('websockets.connect')
    def test_send_call(self, mock_connect):
        # Mock the websocket connection and send call
        mock_ws = MagicMock()
        future = asyncio.Future()
        future.set_result(mock_ws)
        mock_connect.return_value = future
        
        async def run_test():
            await self.client.connect()
            await self.client.clear()
            # Verify something was sent
            self.assertTrue(mock_ws.send.called)

        asyncio.run(run_test())

if __name__ == '__main__':
    unittest.main()
