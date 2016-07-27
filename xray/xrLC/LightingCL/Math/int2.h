#pragma once


#include <cmath>
#include <algorithm>

#include "float2.h"

namespace LightingCL
{
    class int2
    {
    public:
        int2(int xx = 0, int yy = 0) : x(xx), y(yy) {}

        int&  operator [](int i)       { return *(&x + i); }
        int   operator [](int i) const { return *(&x + i); }
        int2  operator-() const        { return int2(-x, -y); }

        int   sqnorm() const           { return x*x + y*y; }

        operator float2()     { return float2((float)x, (float)y); }

        int2& operator += (int2 const& o) { x+=o.x; y+=o.y; return *this; }
        int2& operator -= (int2 const& o) { x-=o.x; y-=o.y; return *this; }
        int2& operator *= (int2 const& o) { x*=o.x; y*=o.y; return *this; }
        int2& operator *= (int c) { x*=c; y*=c; return *this; }

        int x, y;
    };


    inline int2 operator+(int2 const& v1, int2 const& v2)
    {
        int2 res = v1;
        return res+=v2;
    }

    inline int2 operator-(int2 const& v1, int2 const& v2)
    {
        int2 res = v1;
        return res-=v2;
    }

    inline int2 operator*(int2 const& v1, int2 const& v2)
    {
        int2 res = v1;
        return res*=v2;
    }

    inline int2 operator*(int2 const& v1, int c)
    {
        int2 res = v1;
        return res*=c;
    }

    inline int2 operator*(int c, int2 const& v1)
    {
        return operator*(v1, c);
    }

    inline int dot(int2 const& v1, int2 const& v2)
    {
        return v1.x * v2.x + v1.y * v2.y;
    }


    inline int2 vmin(int2 const& v1, int2 const& v2)
    {
        return int2(std::min(v1.x, v2.x), std::min(v1.y, v2.y));
    }

    inline int2 vmax(int2 const& v1, int2 const& v2)
    {
        return int2(std::max(v1.x, v2.x), std::max(v1.y, v2.y));
    }
}