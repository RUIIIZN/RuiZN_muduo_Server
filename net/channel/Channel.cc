#include "Channel.h"

namespace net
{

Channel::Channel(int fd)
  : fd_(fd),
    events_(0),
    revents_(0),
    index_(-1),
    eventHandling_(false),
    addedToLoop_(false),
    loop_(nullptr)
{
}

}