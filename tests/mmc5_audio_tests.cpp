#include "apu.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool ok, const char* msg) { if (!ok) { std::cerr << "FAIL: " << msg << '\n'; std::exit(1); } }
int main() {
 NESAPU apu; apu.reset(); apu.writeMmc5(0x5015,3); apu.writeMmc5(0x5000,0xDF); apu.writeMmc5(0x5002,0x40); apu.writeMmc5(0x5003,0xF8); apu.writeMmc5(0x5004,0x9F); apu.writeMmc5(0x5006,0x60); apu.writeMmc5(0x5007,0xF8);
 check(apu.readMmc5Status()==3,"both MMC5 pulse channels become active"); for(int i=0;i<40000;++i) apu.clock(); auto samples=apu.takeSamples(); bool audible=false; for(auto sample:samples) if(sample>0) audible=true; check(audible,"MMC5 pulses contribute to audio");
 auto saved=apu.saveState(); NESAPU restored; restored.loadState(saved); check(restored.readMmc5Status()==3,"pulse status survives save-state restore"); check(restored.saveState().mmc5Pulse[0].timer==saved.mmc5Pulse[0].timer,"pulse timer survives save-state restore"); restored.writeMmc5(0x5015,1); check(restored.readMmc5Status()==1,"disabling second pulse clears its length");
 NESAPU pcm; pcm.reset(); pcm.writeMmc5(0x5010,0x81); pcm.mmc5PcmRead(0x35); auto pcmState=pcm.saveState(); check(pcmState.mmc5Pcm.output==0x35,"PCM read mode captures PRG data"); pcm.mmc5PcmRead(0); check(pcm.irqPending(),"zero-byte PCM read raises PCM IRQ"); check((pcm.readMmc5PcmStatus()&0x80)!=0,"PCM status reports pending IRQ"); check(!pcm.irqPending(),"reading PCM status acknowledges IRQ"); pcm.writeMmc5(0x5010,0x00); pcm.writeMmc5(0x5011,0x62); check(pcm.saveState().mmc5Pcm.output==0x62,"PCM DAC write mode updates output"); pcm.writeMmc5(0x5010,0x01); pcm.writeMmc5(0x5011,0x11); check(pcm.saveState().mmc5Pcm.output==0x62,"PCM DAC writes are ignored in read mode");
 std::cout<<"MMC5 audio tests passed\n";
}
