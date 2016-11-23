#ifndef __PARTICLE_TYPES__H__
#define __PARTICLE_TYPES__H__

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <math.h>


typedef unsigned int u32;
typedef float        f32;



// =============================================================================

struct Vector2 {
	union {
		struct{ float x, y; };
		struct{ float w, h; };
		float a[2];
	};
	Vector2() : x(0), y(0) {}
	Vector2(float c1, float c2) : x(c1), y(c2) {}
	inline void set(float c1, float c2) { x = c1; y = c2; }
	inline Vector2 &operator += (const Vector2& a) { this->x+=a.x; this->y+=a.y; return *this; }
	inline Vector2 &operator -= (const Vector2& a) { this->x-=a.x; this->y-=a.y; return *this; }
	inline Vector2 &operator *= (const float a) { this->x*=a; this->y*=a; return *this; }
	inline Vector2 operator * (const float a) const { return Vector2(this->x*a, this->y*a); }
	inline Vector2 operator * (const Vector2 &a) const { return Vector2(this->x*a.x, this->y*a.y); }
	inline Vector2 operator + (const float a) const { return Vector2(this->x+a, this->y+a); }
	inline Vector2 operator + (const Vector2 &a) const { return Vector2(this->x+a.x, this->y+a.y); }
	inline Vector2 operator - (const Vector2 &a) const { return Vector2(this->x-a.x, this->y-a.y); }
	inline Vector2 operator - () const { return Vector2(-this->x, -this->y); }
	inline float operator [] (const int ix) const { return a[ix]; }
	inline float &operator [] (const int ix) { return a[ix]; }
	inline float distance(const Vector2 &pv) const {float f1 = x - pv.x, f2 = y - pv.y; return hypot(f1,f2);}
	float lengthSquared() { return x*x+y*y; }
	float length() { return hypot(x,y); }
	Vector2& limit(float max) {
	    const float lengthSquared = (x*x + y*y);
	    if( lengthSquared > max*max && lengthSquared > 0 ) {
	        const float ratio = max/(float)sqrt(lengthSquared);
	        x *= ratio;
	        y *= ratio;
	    }
	    return *this;
	}

	void zero() { x = y = 0; }
	static const int DIM = 2;
};

struct Vector3 {
	union {
		struct{ float x, y, z; };
		struct{ float r, g, b; };
		float a[3];
	};
	Vector3() : x(0), y(0), z(0) {}
	Vector3(float c1, float c2, float c3) : x(c1), y(c2), z(c3) {}
	inline void set(float c1, float c2, float c3) { x = c1; y = c2; z = c3; }
};

struct Color {
	union {
		struct{ float r, g, b, a; };
		float c[4];
	};
	Color() : r(0), g(0), b(0), a(1) {}
	Color(float c1, float c2, float c3, float c4) : r(c1), g(c2), b(c3), a(c4) {}
	inline void set(float c1, float c2, float c3, float c4) { r = c1; g = c2; b = c3; a = c4; }
};

struct Matrix2x2 {
    union {
        float m[2][2];
        float d[4];
    };
};



// =============================================================================

#define check(A, M, ...) if(!(A)) { fprintf(stderr, M, ##__VA_ARGS__); errno=0; goto error; }

typedef struct DArray {
  int end;
  int max;
  size_t element_size;
  size_t expand_rate;
  void **contents;
} DArray;

static inline void _DArraySet(DArray *darray, int index, void *el) {
  check(index < darray->max, "DArray attempt to set past max");
  if(index > darray->end) darray->end = index;
  darray->contents[index] = el;
error:
  return;
}

static inline void *_DArrayGet(DArray *darray, int index) {
  check(index < darray->max, "DArray attempt to get past max");
  return darray->contents[index];
error:
  return NULL;
}

static inline int _DArrayLength(DArray *darray) {
  return darray->end;
}

static inline void *DArrayRemove(DArray *darray, int index) {
  check(index < darray->max, "DArray attempt to remove past max");
  {
      void *el = darray->contents[index];
      darray->contents[index] = NULL;
      return el;
  }
error:
  return NULL;
}

static inline void *DArrayNew(DArray *darray) {
  check(darray->element_size > 0, "0 size dynamic darray");
  return malloc(darray->element_size);
error:
  return NULL;
}

static inline DArray *DArrayCreate(size_t element_size, int max, int expand_rate = -1) {
  check(max > 0, "initial max must be > 0");
  {
      DArray *darray = (DArray *)malloc(sizeof(DArray));
      darray->element_size = element_size;
      darray->expand_rate = (expand_rate==-1) ? max * 2 : expand_rate;
      darray->max = max;
      darray->contents = (void**)calloc(max, sizeof(void *));
      darray->end = 0;
      return darray;
  }
error:
  return NULL;
}

static inline int _DArrayResize(DArray *darray, int new_size) {
  darray->max = new_size;
  check(darray->max > 0, "The newsize must be > 0.");
  {
      void **contents = (void**)realloc(darray->contents, new_size * sizeof(void *));
      darray->contents = contents;
      return 1;
  }
error:
  return 0;
}

static inline void DArrayExpand(DArray *darray) {
  int old_max = darray->max;
  if(_DArrayResize(darray, old_max + (int)darray->expand_rate)) {
    memset(darray->contents + old_max, 0, darray->expand_rate + 1);
  }
}

static inline void DArrayContract(DArray *darray) {
  int new_size = darray->end < (int)darray->expand_rate ? (int)darray->expand_rate : darray->end;
  _DArrayResize(darray, new_size + 1);
}

static inline void * DArrayInc(DArray *darray) {
  int old_end = darray->end;
  darray->end++;
  if(darray->end >= darray->max) {
    DArrayExpand(darray);
  }
  return darray->contents[old_end];
}

static inline void DArrayPush(DArray *darray, void *el) {
  darray->contents[darray->end] = el;
  darray->end++;
  if(darray->end >= darray->max) {
    DArrayExpand(darray);
  }
}

static inline void *DArrayPop(DArray *darray) {
  check(darray->end > 0, "trying to pop from empty array");
  {
      void *el = DArrayRemove(darray, darray->end - 1);
      darray->end--;
      return el;
  }
error:
  return NULL;
}

static inline void DArrayClear(DArray *darray) {
  for(int i=0; i<=darray->max; i++) {
    if(darray->contents[i] != NULL) {
      free(darray->contents[i]);
    }
  }
}

static inline void DArrayDestroy(DArray *darray) {
  free(darray->contents);
  free(darray);
}



// =============================================================================

class FastRand
{
public:
    FastRand() { srandom(time(0)); Seed(random(), random()); }
	FastRand(uint32_t seed1, uint32_t seed2 = 0);
    FastRand(uint64_t seed);
	void Seed(uint32_t seed1, uint32_t seed2 = 0);
	void Seed(uint64_t seed);
	uint32_t Rand();
    uint32_t operator()();
    int Rand(int min, int max);
    float  RandF();
    double RandD();
    float RandF(float max);
    float RandF(float min, float max);
    double RandD(double min, double max);
public:
    static FastRand *R();
private:
	uint32_t m_High;
	uint32_t m_Low;
};

FastRand *FastRand::R() {
    static FastRand _fr;
    return &_fr;
}
inline FastRand::FastRand(uint32_t seed1, uint32_t seed2) : m_High(seed1), m_Low(seed2) {
    if( m_Low == 0 ) m_Low = m_High ^ 0x49616E42;
}
inline FastRand::FastRand(uint64_t seed) : m_High( (uint32_t)(seed >> 32) ), m_Low( (uint32_t)seed ) { }
inline void FastRand::Seed(uint32_t seed1, uint32_t seed2) {
    m_High = seed1; if( seed2 == 0 )
        m_Low = m_High ^ 0x49616E42;
    else
        m_Low = seed2;
}
inline void FastRand::Seed(uint64_t seed) { m_High = (uint32_t)(seed >> 32); m_Low = (uint32_t)seed; }
inline uint32_t FastRand::Rand() {
    m_High	= (m_High<<16) + (m_High>>16);
    m_High	+= m_Low; m_Low	+= m_High;
    return m_High;
}
inline uint32_t FastRand::operator()() { return Rand(); }
inline int FastRand::Rand(int min, int max) { return Rand() % (max-min) + min; }
inline float FastRand::RandF() { return Rand()*(1.0f/4294967295.0f); }
inline double FastRand::RandD() {
   unsigned long a=Rand()>>5, b=Rand()>>6; 
   return(a*67108864.0+b)*(1.0/9007199254740992.0); 
}
inline float FastRand::RandF(float max) { return RandF() * max; }
inline float FastRand::RandF(float min, float max) { return RandF() * (max-min) + min; }
inline double FastRand::RandD(double min, double max) { return RandD() * (max-min) + min; }



// =============================================================================

namespace {
    inline static float MathCos(float a) { return ::cosf(a); } 
    inline static double MathCos(double a) { return ::cos(a); } 
    inline static float MathSin(float a) { return ::sinf(a); } 
    inline static double MathSin(double a) { return ::sin(a); } 
}

#define RANDFLOATMAX(m) FastRand::R()->RandF(m)
#define RANDFLOATRANGE(m1, m2) FastRand::R()->RandF(m1, m2)
#define PRINTF printf

#endif // __PARTICLE_TYPES__H__
