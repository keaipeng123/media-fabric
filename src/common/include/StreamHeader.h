#ifndef _STREAMHEADER_H
#define _STREAMHEADER_H

struct StreamHeader
{
    char type;
    int pts;
    int length;
    int keyFrame;
};

#endif
