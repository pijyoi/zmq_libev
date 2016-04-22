#include <memory>

#include <stdio.h>

#include "uvpp.hpp"

int main()
{
  uvpp::Loop uvloop;

  uvpp::Signal usignal(uvloop);
  usignal.set_callback([&uvloop](){
    uvloop.stop();
  });
  usignal.start(SIGINT);

  int cnt = 0;
  uvpp::Timer utimer(uvloop);
  utimer.set_callback([&cnt](){
    printf("%d\n", cnt++);
  });
  utimer.start(0, 1000);

  uvloop.run();
  printf("first loop break\n");
}
