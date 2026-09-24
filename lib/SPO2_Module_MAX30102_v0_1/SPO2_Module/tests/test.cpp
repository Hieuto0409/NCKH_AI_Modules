#include "ResearchSpO2.h"
#include "MaximCore.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
using namespace research_spo2;
int main() {
 uint32_t ir[100],red[100];
 for(int i=0;i<100;++i) { ir[i]=uint32_t(60000+1200*std::sin(i*2*3.141592653589793/20));red[i]=uint32_t(40000+500*std::sin(i*2*3.141592653589793/20)); }
 assert(calculate25(ir,red,100,true).valid());
 assert(calculate25(nullptr,red,100,true).status==Status::InvalidInput);
 assert(calculate25(ir,red,99,true).status==Status::InvalidInput);
 assert(calculate25(ir,red,100,false).status==Status::QualityRejected);
 uint32_t old=ir[5]; ir[5]=262143; assert(calculate25(ir,red,100,true).status==Status::Clipped);
 ir[5]=262144; assert(calculate25(ir,red,100,true).status==Status::InvalidInput);
 ir[5]=0; assert(calculate25(ir,red,100,true).status==Status::ContactLost); ir[5]=old;
 Stream100 stream;
 for(int i=0;i<399;++i) stream.push(ir[(i/4)%100],red[(i/4)%100]);
 assert(stream.evaluate(true).status==Status::NeedData);
 stream.push(ir[99],red[99]);assert(stream.evaluate(true).valid());
 assert(stream.evaluate(false).status==Status::QualityRejected);
 stream.push(ir[0],red[0]);stream.push(0,0); // Glitch on a discarded sample.
 assert(stream.evaluate(true).status==Status::ContactLost);
 for(int i=0;i<400;++i) stream.push(ir[(i/4)%100],red[(i/4)%100]);
 assert(stream.evaluate(true).valid());stream.reset();assert(!stream.evaluate(true).valid());
 for(int i=0;i<100;++i) ir[i]=red[i]=50000;
 assert(calculate25(ir,red,100,true).status==Status::AlgorithmRejected);
 int32_t plateau[100]={}; for(int i=90;i<100;++i) plateau[i]=100;
 int32_t loc[15],n=0; research_spo2_core::maxim_peaks_above_min_height(loc,&n,plateau,100,30);
 assert(n==0); // Plateau reaches the final element; must not read element 100.
 std::mt19937 rng(42);
 for(int k=0;k<3000;++k) {
  for(int i=0;i<100;++i) {ir[i]=10000+rng()%252143;red[i]=1+rng()%262142;}
  const Result r=calculate25(ir,red,100,true);
  assert(!r.valid() || (r.percent>=0 && r.percent<=100));
 }
 for(int s=1;s<=3;++s) {
  std::ifstream f("tests/data/session_"+std::to_string(s)+".csv");assert(f.good());
  std::string line;std::getline(f,line);Stream100 m;int count=0,j=0;
  while(std::getline(f,line)) { const auto comma=line.find(',');const uint32_t x=std::stoul(line.substr(0,comma)),y=std::stoul(line.substr(comma+1));
   m.push(x,y);if(count>=5600 && count%4==0){ir[j]=x;red[j++]=y;}++count;
  }
  assert(count==6000 && j==100);const auto r=m.evaluate(true),b=calculate25(ir,red,100,true);
  assert(r.valid() && r.percent==b.percent);
  const int expected[3]={99,100,99}; assert(r.percent==expected[s-1]);
  std::cout<<"Replay session "<<s<<": "<<r.percent<<"%, HR="<<r.heartRate<<"\n";
 }
 std::cout<<"PASS: input/state tests, tail plateau, 3000 random windows, 3 real sessions\n";
}
