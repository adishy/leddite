import struct

def create_packet(width, height, x=0, y=0, rotation=0, flags=0, brightness=255, pixels=None):
    # Header: version(1), flags(1), width(1), height(1), x(1), y(1), rot(1), bright(1)
    header = struct.pack('BBBBbbBB', 1, flags, width, height, x, y, rotation, brightness)
    
    if pixels is None:
        # Default: full white
        pixels = [255, 255, 255] * (width * height)
    
    return header + bytes(pixels)

def to_js_array(packet):
    return "new Uint8Array([" + ", ".join(map(str, packet)) + "])"

# Example: 4x4 red square at (2, 2)
red_pixels = [255, 0, 0] * 16
packet = create_packet(4, 4, x=2, y=2, pixels=red_pixels)
print("--- 4x4 Red Square at (2,2) ---")
print(to_js_array(packet))

# Example: 1x8 rainbow strip rotated 90 deg
rainbow = []
for i in range(8):
    rainbow.extend([i * 32, 255 - i * 32, 128])
packet_rainbow = create_packet(1, 8, x=5, y=5, rotation=1, pixels=rainbow)
print("\n--- 1x8 Rainbow Strip Rotated 90 at (5,5) ---")
print(to_js_array(packet_rainbow))
