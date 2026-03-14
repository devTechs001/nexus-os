#!/usr/bin/env python3
import struct

with open('build/nexus-kernel.bin', 'rb') as f:
    data = f.read(65536)

magic = b'\xd6\x50\x52\xe8'
offset = data.find(magic)

if offset == -1:
    print("ERROR: Multiboot2 magic not found!")
else:
    print(f"Multiboot2 header found at offset: 0x{offset:X} ({offset})")
    
    if offset + 16 <= len(data):
        magic_val = struct.unpack('<I', data[offset:offset+4])[0]
        arch = struct.unpack('<I', data[offset+4:offset+8])[0]
        header_len = struct.unpack('<I', data[offset+8:offset+12])[0]
        checksum = struct.unpack('<I', data[offset+12:offset+16])[0]
        
        print(f"  Magic: 0x{magic_val:08X}")
        print(f"  Architecture: 0x{arch:08X} ({'i386' if arch == 0 else 'other'})")
        print(f"  Header Length: {header_len} bytes")
        print(f"  Checksum: 0x{checksum:08X}")
        
        total = (magic_val + arch + header_len + checksum) & 0xFFFFFFFF
        print(f"  Checksum verification: {'PASS' if total == 0 else 'FAIL'} (sum = 0x{total:08X})")
        
        print("\n  Tags:")
        pos = offset + 16
        while pos < offset + header_len:
            tag_type = struct.unpack('<H', data[pos:pos+2])[0]
            tag_flags = struct.unpack('<H', data[pos+2:pos+4])[0]
            tag_size = struct.unpack('<I', data[pos+4:pos+8])[0]
            
            tag_names = {0: 'End', 1: 'Information Request', 2: 'Address', 3: 'Entry Address',
                        4: 'Control Address', 5: 'Framebuffer', 6: 'Module Align',
                        7: 'EFI BS', 8: 'EFI Entry', 9: 'Relocatable'}
            tag_name = tag_names.get(tag_type, f'Unknown({tag_type})')
            print(f"    - {tag_name} (flags=0x{tag_flags:X}, size={tag_size})")
            
            if tag_type == 0:
                break
            pos += (tag_size + 7) & ~7
