#include "cartridge.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool ok, const char* msg) { if (!ok) { std::cerr << "FAIL: " << msg << '\n'; std::exit(1); } }
int main() {
    Cartridge cart; cart.mapper = 11;
    cart.prgRom.resize(4 * 0x8000);
    for (std::size_t bank=0; bank<4; ++bank)
        for (std::size_t i=0; i<0x8000; ++i) cart.prgRom[bank*0x8000+i]=static_cast<std::uint8_t>(bank+0x10);
    cart.chrData.resize(4 * 0x2000);
    for (std::size_t bank=0; bank<4; ++bank)
        for (std::size_t i=0; i<0x2000; ++i) cart.chrData[bank*0x2000+i]=static_cast<std::uint8_t>(bank+0x40);
    cart.writePrg(0x8000,0x21);
    check(cart.readPrg(0x8000)==0x11,"selects 32 KiB PRG bank");
    check(cart.readPrg(0xFFFF)==0x11,"bank spans full CPU window");
    check(cart.readChr(0)==0x42 && cart.readChr(0x1FFF)==0x42,"selects 8 KiB CHR bank");
    const auto saved=cart.saveState(); cart.writePrg(0x8000,0x30);
    check(cart.readPrg(0x8000)==0x10,"PRG bank switches");
    check(cart.loadState(saved),"mapper state restores");
    check(cart.readPrg(0x8000)==0x11 && cart.readChr(0)==0x42,"bank registers survive state restore");
    Cartridge ram; ram.mapper=11; ram.usesChrRam=true; ram.chrData.resize(0x2000);
    ram.writePrg(0x8000,0x10); ram.writeChr(0,0xA5);
    check(ram.readChr(0)==0xA5,"supports CHR RAM");
    std::cout<<"Mapper 11 tests passed\n";
}