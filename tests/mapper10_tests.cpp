#include "cartridge.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
static void check(bool ok, const char* msg) { if (!ok) { std::cerr << "FAIL: " << msg << '\n'; std::exit(1); } }
int main() {
    Cartridge c; c.mapper=10; c.prgRom.resize(4*0x4000);
    for (std::size_t b=0;b<4;++b) std::fill(c.prgRom.begin()+b*0x4000,c.prgRom.begin()+(b+1)*0x4000,static_cast<std::uint8_t>(0x20+b));
    c.chrData.resize(8*0x1000);
    for (std::size_t b=0;b<8;++b) std::fill(c.chrData.begin()+b*0x1000,c.chrData.begin()+(b+1)*0x1000,static_cast<std::uint8_t>(0x50+b));
    c.writePrg(0xA000,2);
    check(c.readPrg(0x8000)==0x22 && c.readPrg(0xBFFF)==0x22,"switchable lower 16 KiB bank");
    check(c.readPrg(0xC000)==0x23 && c.readPrg(0xFFFF)==0x23,"fixed upper 16 KiB bank");
    c.writePrg(0xB000,1); c.writePrg(0xC000,2); c.writePrg(0xD000,3); c.writePrg(0xE000,4);
    check(c.readChr(0)==0x51,"lower FD latch bank");
    c.readChr(0x0FE8); check(c.readChr(0)==0x52,"lower FE latch bank");
    check(c.readChr(0x1000)==0x53,"upper FD latch bank");
    c.readChr(0x1FE8); check(c.readChr(0x1000)==0x54,"upper FE latch bank");
    const auto state=c.saveState(); c.readChr(0x0FD8); c.writePrg(0xA000,1);
    check(c.loadState(state),"state restores");
    check(c.readPrg(0x8000)==0x22 && c.readChr(0)==0x52 && c.readChr(0x1000)==0x54,"bank and latch state restored");
    std::cout<<"Mapper 10 tests passed\n";
}
