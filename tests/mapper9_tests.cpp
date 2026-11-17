#include "cartridge.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
static void check(bool ok, const char* msg) { if (!ok) { std::cerr << "FAIL: " << msg << '\n'; std::exit(1); } }
int main() {
    Cartridge c; c.mapper=9; c.prgRom.resize(8*0x2000);
    for (std::size_t b=0;b<8;++b) std::fill(c.prgRom.begin()+b*0x2000,c.prgRom.begin()+(b+1)*0x2000,static_cast<std::uint8_t>(0x10+b));
    c.chrData.resize(8*0x1000);
    for (std::size_t b=0;b<8;++b) std::fill(c.chrData.begin()+b*0x1000,c.chrData.begin()+(b+1)*0x1000,static_cast<std::uint8_t>(0x40+b));
    c.writePrg(0xA000,3);
    check(c.readPrg(0x8000)==0x13,"switchable PRG bank");
    check(c.readPrg(0xA000)==0x15 && c.readPrg(0xC000)==0x16 && c.readPrg(0xE000)==0x17,"fixed PRG banks");
    c.writePrg(0xB000,1); c.writePrg(0xC000,2); c.writePrg(0xD000,3); c.writePrg(0xE000,4);
    check(c.readChr(0)==0x41,"lower FD latch bank");
    c.readChr(0x0FE8); check(c.readChr(0)==0x42,"lower FE latch switches");
    check(c.readChr(0x1000)==0x43,"upper FD latch bank");
    c.readChr(0x1FE8); check(c.readChr(0x1000)==0x44,"upper FE latch switches");
    const auto state=c.saveState(); c.readChr(0x0FD8); c.writePrg(0xA000,1);
    check(c.loadState(state),"state restores");
    check(c.readPrg(0x8000)==0x13 && c.readChr(0)==0x42 && c.readChr(0x1000)==0x44,"banks and latches restored");
    std::cout<<"Mapper 9 tests passed\n";
}
