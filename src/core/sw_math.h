#pragma once

#include <math.h>
#include <float.h>

#define SW_DTOR (static_cast<float>(M_PI / 180))
#define SW_RTOD (static_cast<float>(180 / M_PI))
#define SW_PI (static_cast<float>(M_PI))
#define SW_2PI (static_cast<float>(2 * M_PI))
#define SW_RGB_EPSILON (static_cast<float>(0.5f / 255.0f))

inline constexpr float SW_EPSILON = 0.0001f;

#define SW_RGB(r,g,b) SW_RGBA(r,g,b,255)
#define SW_RGBA(r,g,b,a) ((quint32)(((quint8) (r) | ((quint16) (g) << 8)) | (((quint32) (quint8) (b)) << 16) | (((quint32) (quint8) (a)) << 24)))
#define SW_RGBA_RED(rgba)   ((quint8)(((rgba) >>  0) & 0xff))
#define SW_RGBA_GREEN(rgba) ((quint8)(((rgba) >>  8) & 0xff))
#define SW_RGBA_BLUE(rgba)  ((quint8)(((rgba) >> 16) & 0xff))
#define SW_RGBA_ALPHA(rgba) ((quint8)(((rgba) >> 24) & 0xff))
#define SW_SRGB_TO_LINEAR(v) (powf(v, 2.2f))
#define SW_LINEAR_TO_SRGB(v) (powf(v, 1.0f / 2.2f))

inline quint32 swRGBAFromQColor(const QColor& Color)
{
	return SW_RGBA(Color.red(), Color.green(), Color.blue(), Color.alpha());
}

inline QColor swQColorFromRGBA(quint32 RGBA)
{
	return QColor::fromRgb(SW_RGBA_RED(RGBA), SW_RGBA_GREEN(RGBA), SW_RGBA_BLUE(RGBA), SW_RGBA_ALPHA(RGBA));
}

template<typename T>
inline T swMin(const T& a, const T& b)
{
	return a < b ? a : b;
}

template<typename T>
inline T swMax(const T& a, const T& b)
{
	return a > b ? a : b;
}

template<typename T>
inline T swClamp(const T& Value, const T& Min, const T& Max)
{
	if (Value > Max)
		return Max;
	else if (Value < Min)
		return Min;
	else
		return Value;
}

inline bool swIsZero(float Value)
{
	return fabsf(Value) < SW_EPSILON;
}

class swVector2i
{
public:
	swVector2i()
	{
	}

	constexpr swVector2i(const int _x, const int _y)
		: x(_x), y(_y)
	{
	}

	int x, y;
};

inline bool operator==(const swVector2i& a, const swVector2i& b)
{
	return a.x == b.x && a.y == b.y;
}

inline bool operator!=(const swVector2i& a, const swVector2i& b)
{
	return a.x != b.x || a.y != b.y;
}

class swVector2
{
public:
	swVector2()
	{
	}

	constexpr swVector2(const float _x, const float _y)
		: x(_x), y(_y)
	{
	}

	const float& operator[](int i) const
	{
		return ((float*)this)[i];
	}

	float& operator[](int i)
	{
		return ((float*)this)[i];
	}

	const float* GetFloats() const
	{
		return (const float*)this;
	}

	float* GetFloats()
	{
		return (float*)this;
	}

	bool IsNan() const
	{
		return std::isnan(x) || std::isnan(y);
	}

	float x, y;
};

class swVector3
{
public:
	swVector3()
	{
	}

	constexpr swVector3(const float _x, const float _y, const float _z)
		: x(_x), y(_y), z(_z)
	{
	}

	explicit swVector3(const swVector4& v);

	const float& operator[](int i) const
	{
		return ((float*)this)[i];
	}

	float& operator[](int i)
	{
		return ((float*)this)[i];
	}

	const float* GetFloats() const
	{
		return (const float*)this;
	}

	float* GetFloats()
	{
		return (float*)this;
	}

	bool IsNan() const
	{
		return std::isnan(x) || std::isnan(y) || std::isnan(z);
	}

	void Normalize();
	float Length() const;
	float LengthSquared() const;

	float x, y, z;
};

class swVector4
{
public:
	swVector4()
	{
	}

	constexpr swVector4(const float _x, const float _y, const float _z, const float _w)
		: x(_x), y(_y), z(_z), w(_w)
	{
	}

	constexpr swVector4(const swVector3& _xyz, const float _w)
		: x(_xyz.x), y(_xyz.y), z(_xyz.z), w(_w)
	{
	}

	const float& operator[](int i) const
	{
		return ((float*)this)[i];
	}

	float& operator[](int i)
	{
		return ((float*)this)[i];
	}

	const float* GetFloats() const
	{
		return (const float*)this;
	}

	float* GetFloats()
	{
		return (float*)this;
	}

	bool IsNan() const
	{
		return std::isnan(x) || std::isnan(y) || std::isnan(z) || std::isnan(w);
	}

	float x, y, z, w;
};

class swMatrix33
{
public:
	swMatrix33()
	{
	}

	swMatrix33(const swVector3& _x, const swVector3& _y, const swVector3& _z)
	{
		r[0] = _x;
		r[1] = _y;
		r[2] = _z;
	}

	explicit swMatrix33(const swMatrix44& Matrix);

	const swVector3& operator[](int i) const
	{
		return r[i];
	}

	swVector3& operator[](int i)
	{
		return r[i];
	}

	const float* GetFloats() const
	{
		return (const float*)this;
	}

	float* GetFloats()
	{
		return (float*)this;
	}

	void Orthonormalize();

	swVector3 r[3];
};

class swMatrix44
{
public:
	swMatrix44()
	{
	}

	swMatrix44(const swVector4& _x, const swVector4& _y, const swVector4& _z, const swVector4& _w)
	{
		r[0] = _x;
		r[1] = _y;
		r[2] = _z;
		r[3] = _w;
	}

	swMatrix44(const swMatrix33& Rotation, const swVector3& Translation)
	{
		r[0] = swVector4(Rotation[0][0], Rotation[0][1], Rotation[0][2], 0.0f);
		r[1] = swVector4(Rotation[1][0], Rotation[1][1], Rotation[1][2], 0.0f);
		r[2] = swVector4(Rotation[2][0], Rotation[2][1], Rotation[2][2], 0.0f);
		r[3] = swVector4(Translation, 1.0f);
	}

	swVector3 GetTranslation() const
	{
		return swVector3(r[3][0], r[3][1], r[3][2]);
	}

	void SetTranslation(const swVector3& Translation)
	{
		r[3] = swVector4(Translation[0], Translation[1], Translation[2], 1.0f);
	}

	const swVector4& operator[](int i) const
	{
		return r[i];
	}

	swVector4& operator[](int i)
	{
		return r[i];
	}

	const float* GetFloats() const
	{
		return (const float*)this;
	}

	float* GetFloats()
	{
		return (float*)this;
	}

	float Determinant() const;

	swVector4 r[4];
};

inline bool operator==(const swVector2& a, const swVector2& b)
{
	return a.x == b.x && a.y == b.y;
}

inline swVector3::swVector3(const swVector4& v)
	: x(v.x), y(v.y), z(v.z)
{
}

inline swVector3 operator+(const swVector3& a, const swVector3& b)
{
	return swVector3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline swVector3 operator-(const swVector3& a, const swVector3& b)
{
	return swVector3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline swVector3 operator*(const swVector3& a, const swVector3& b)
{
	return swVector3(a.x * b.x, a.y * b.y, a.z * b.z);
}

inline swVector3 operator/(const swVector3& a, const swVector3& b)
{
	return swVector3(a.x / b.x, a.y / b.y, a.z / b.z);
}

inline swVector3 operator*(const swVector3& a, float b)
{
	return swVector3(a.x * b, a.y * b, a.z * b);
}

inline swVector3 operator/(const swVector3& a, float b)
{
	return swVector3(a.x / b, a.y / b, a.z / b);
}

inline swVector3 operator*(float a, const swVector3& b)
{
	return swVector3(b.x * a, b.y * a, b.z * a);
}

inline swVector3 operator/(float a, const swVector3& b)
{
	return swVector3(b.x / a, b.y / a, b.z / a);
}

inline swVector3 operator-(const swVector3& a)
{
	return swVector3(-a.x, -a.y, -a.z);
}

inline swVector3& operator+=(swVector3& a, const swVector3& b)
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;

	return a;
}

inline swVector3& operator-=(swVector3& a, const swVector3& b)
{
	a.x -= b.x;
	a.y -= b.y;
	a.z -= b.z;

	return a;
}

inline swVector3& operator*=(swVector3& a, const swVector3& b)
{
	a.x *= b.x;
	a.y *= b.y;
	a.z *= b.z;

	return a;
}

inline swVector3& operator/=(swVector3& a, const swVector3& b)
{
	a.x /= b.x;
	a.y /= b.y;
	a.z /= b.z;

	return a;
}

inline swVector3& operator+=(swVector3& a, float b)
{
	a.x += b;
	a.y += b;
	a.z += b;

	return a;
}

inline swVector3& operator*=(swVector3& a, float b)
{
	a.x *= b;
	a.y *= b;
	a.z *= b;

	return a;
}

inline swVector3& operator/=(swVector3& a, float b)
{
	a.x /= b;
	a.y /= b;
	a.z /= b;

	return a;
}

inline bool operator==(const swVector3& a, const swVector3& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool operator!=(const swVector3& a, const swVector3& b)
{
	return a.x != b.x || a.y != b.y || a.z != b.z;
}

inline bool operator<(const swVector3& a, const swVector3& b)
{
	if (a.x < b.x)
		return true;
	else if (a.x > b.x)
		return false;

	if (a.y < b.y)
		return true;
	else if (a.y > b.y)
		return false;

	if (a.z < b.z)
		return true;
	else if (a.z > b.z)
		return false;

	return false;
}

inline bool operator==(const swVector4& a, const swVector4& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

inline bool operator==(const swMatrix33& a, const swMatrix33& b)
{
	return a.r[0] == b.r[0] && a.r[1] == b.r[1] && a.r[2] == b.r[2];
}

inline bool operator==(const swMatrix44& a, const swMatrix44& b)
{
	return a.r[0] == b.r[0] && a.r[1] == b.r[1] && a.r[2] == b.r[2] && a.r[3] == b.r[3];
}

#ifndef QT_NO_DEBUG

inline QDebug operator<<(QDebug Debug, const swVector2& v)
{
	QDebugStateSaver Saver(Debug);
	Debug.nospace() << '(' << v.x << ", " << v.y << ')';
	return Debug;
}

inline QDebug operator<<(QDebug Debug, const swVector3& v)
{
	QDebugStateSaver Saver(Debug);
	Debug.nospace() << '(' << v.x << ", " << v.y << ", " << v.z << ')';
	return Debug;
}

inline QDebug operator<<(QDebug Debug, const swVector4& v)
{
	QDebugStateSaver Saver(Debug);
	Debug.nospace() << '(' << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ')';
	return Debug;
}

inline QDebug operator<<(QDebug Debug, const swMatrix33& m)
{
	QDebugStateSaver Saver(Debug);
	Debug.nospace() << '[' << m[0] << ", " << m[1] << ", " << m[2] << ']';
	return Debug;
}

inline QDebug operator<<(QDebug Debug, const swMatrix44& m)
{
	QDebugStateSaver Saver(Debug);
	Debug.nospace() << '[' << m[0] << ", " << m[1] << ", " << m[2] << ", " << m[3] << ']';
	return Debug;
}

#endif

inline QDataStream& operator<<(QDataStream& Stream, const swVector3& v)
{
	Stream << v.x << v.y << v.z;
	return Stream;
}

inline QDataStream& operator>>(QDataStream& Stream, swVector3& v)
{
	Stream >> v.x >> v.y >> v.z;
	return Stream;
}

inline QDataStream& operator<<(QDataStream& Stream, const swVector4& v)
{
	Stream << v.x << v.y << v.z << v.w;
	return Stream;
}

inline QDataStream& operator>>(QDataStream& Stream, swVector4& v)
{
	Stream >> v.x >> v.y >> v.z >> v.w;
	return Stream;
}

inline QDataStream& operator<<(QDataStream& Stream, const swMatrix33& m)
{
	Stream << m[0] << m[1] << m[2];
	return Stream;
}

inline QDataStream& operator>>(QDataStream& Stream, swMatrix33& m)
{
	Stream >> m[0] >> m[1] >> m[2];
	return Stream;
}

inline QDataStream& operator<<(QDataStream& Stream, const swMatrix44& m)
{
	Stream << m[0] << m[1] << m[2] << m[3];
	return Stream;
}

inline QDataStream& operator>>(QDataStream& Stream, swMatrix44& m)
{
	Stream >> m[0] >> m[1] >> m[2] >> m[3];
	return Stream;
}

inline void swVector3::Normalize()
{
	const float InvLength = 1.0f / Length();

	x *= InvLength;
	y *= InvLength;
	z *= InvLength;
}

inline float swVector3::Length() const
{
	return sqrtf(x * x + y * y + z * z);
}

inline float swVector3::LengthSquared() const
{
	return x * x + y * y + z * z;
}

inline float swLength(const swVector3& a)
{
	return a.Length();
}

inline float swLengthSquared(const swVector3& a)
{
	return a.LengthSquared();
}

inline bool swIsZero(const swVector3& Value)
{
	return Value.LengthSquared() < SW_EPSILON * SW_EPSILON;
}

inline swVector3 swNormalize(const swVector3& a)
{
	swVector3 Ret(a);
	Ret.Normalize();
	return Ret;
}

inline float swDot(const swVector3& a, const swVector3& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float swDot3(const swVector4& a, const swVector3& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float swDot3(const swVector3& a, const swVector4& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float swDot3(const swVector4& a, const swVector4& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float swDot(const swVector4& a, const swVector4& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

inline swVector3 swCross(const swVector3& a, const swVector3& b)
{
	return swVector3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

template<>
inline swVector3 swMin<swVector3>(const swVector3& a, const swVector3& b)
{
	return swVector3(a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z);
}

template<>
inline swVector3 swMax<swVector3>(const swVector3& a, const swVector3& b)
{
	return swVector3(a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z);
}

inline swVector4 operator+(const swVector4& a, const swVector4& b)
{
	return swVector4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

inline swVector4 operator-(const swVector4& a, const swVector4& b)
{
	return swVector4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

inline swVector4 operator*(const swVector4& a, float f)
{
	return swVector4(a.x * f, a.y * f, a.z * f, a.w * f);
}

inline swVector4 operator*(const swVector4& a, const swVector4& b)
{
	return swVector4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

inline swVector4 operator/(const swVector4& a, float f)
{
	return swVector4(a.x / f, a.y / f, a.z / f, a.w / f);
}

inline swVector4 operator/(const swVector4& a, const swVector4& b)
{
	return swVector4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}

inline swVector4& operator+=(swVector4& a, const swVector4& b)
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
	a.w += b.w;

	return a;
}

inline swVector4& operator-=(swVector4& a, const swVector4& b)
{
	a.x -= b.x;
	a.y -= b.y;
	a.z -= b.z;
	a.w -= b.w;

	return a;
}

inline swVector4& operator*=(swVector4& a, float b)
{
	a.x *= b;
	a.y *= b;
	a.z *= b;
	a.w *= b;

	return a;
}

inline swVector4& operator/=(swVector4& a, float b)
{
	a.x /= b;
	a.y /= b;
	a.z /= b;
	a.w /= b;

	return a;
}

inline quint32 swPackNormal(const swVector3& Normal)
{
	quint32 Packed = 0;

	Packed |= (((qint8)(Normal.x * 127.0f)) & 0xff) << 0;
	Packed |= (((qint8)(Normal.y * 127.0f)) & 0xff) << 8;
	Packed |= (((qint8)(Normal.z * 127.0f)) & 0xff) << 16;

	return Packed;
}

inline swVector3 swUnpackNormal(quint32 Packed)
{
	swVector3 Normal;

	Normal.x = (float)(qint8)((Packed >>  0) & 0xff) / 127.0f;
	Normal.y = (float)(qint8)((Packed >>  8) & 0xff) / 127.0f;
	Normal.z = (float)(qint8)((Packed >> 16) & 0xff) / 127.0f;

	return Normal;
}

inline swVector3 swVector3LDrawToStudWorks(const swVector3& Vector)
{
	return swVector3(Vector[0], Vector[2], -Vector[1]);
}

inline swVector3 swVector3FromColor(quint32 Color)
{
	swVector3 v(SW_RGBA_RED(Color), SW_RGBA_GREEN(Color), SW_RGBA_BLUE(Color));
	v /= 255.0f;
	return v;
}

inline swVector3 swVector3FromQColor(QColor Color)
{
	return swVector3(Color.redF(), Color.greenF(), Color.blueF());
}

inline swVector4 swVector4FromColor(quint32 Color)
{
	swVector4 v(SW_RGBA_RED(Color), SW_RGBA_GREEN(Color), SW_RGBA_BLUE(Color), SW_RGBA_ALPHA(Color));
	v /= 255.0f;
	return v;
}

inline quint32 swColorFromVector3(const swVector3& Color)
{
	return SW_RGB(roundf(Color[0] * 255), roundf(Color[1] * 255), roundf(Color[2] * 255));
}

inline QColor swQColorFromVector3(const swVector3& Color)
{
	return QColor::fromRgb(roundf(Color[0] * 255), roundf(Color[1] * 255), roundf(Color[2] * 255));
}

inline float swLuminescence(const swVector3& Color)
{
	return 0.2126f * Color[0] + 0.7152f * Color[1] + 0.0722f * Color[2];
}

inline swVector3 swSRGBToLinear(const swVector3& Color)
{
	const float r = SW_SRGB_TO_LINEAR(Color[0]);
	const float g = SW_SRGB_TO_LINEAR(Color[1]);
	const float b = SW_SRGB_TO_LINEAR(Color[2]);

	return swVector3(r, g, b);
}

inline swVector3 swLinearToSRGB(const swVector3& Color)
{
	const float r = SW_LINEAR_TO_SRGB(Color[0]);
	const float g = SW_LINEAR_TO_SRGB(Color[1]);
	const float b = SW_LINEAR_TO_SRGB(Color[2]);

	return swVector3(r, g, b);
}

inline swVector3 swMul(const swVector3& a, const swMatrix33& b)
{
	return b.r[0] * a[0] + b.r[1] * a[1] + b.r[2] * a[2];
}

inline swVector3 swMul31(const swVector3& a, const swMatrix44& b)
{
	swVector4 v = b.r[0] * a[0] + b.r[1] * a[1] + b.r[2] * a[2] + b.r[3];

	return swVector3(v[0], v[1], v[2]);
}

inline swVector3 swMul31(const swVector4& a, const swMatrix44& b)
{
	swVector4 v = b.r[0] * a[0] + b.r[1] * a[1] + b.r[2] * a[2] + b.r[3];

	return swVector3(v[0], v[1], v[2]);
}

inline swVector3 swMul30(const swVector3& a, const swMatrix44& b)
{
	swVector4 v = b.r[0] * a[0] + b.r[1] * a[1] + b.r[2] * a[2];

	return swVector3(v[0], v[1], v[2]);
}

inline swVector3 swMul30(const swVector4& a, const swMatrix44& b)
{
	swVector4 v = b.r[0] * a[0] + b.r[1] * a[1] + b.r[2] * a[2];

	return swVector3(v[0], v[1], v[2]);
}

inline swVector4 swMul4(const swVector4& a, const swMatrix44& b)
{
	return b.r[0] * a[0] + b.r[1] * a[1] + b.r[2] * a[2] + b.r[3] * a[3];
}

inline swMatrix33 swMul(const swMatrix33& a, const swMatrix33& b)
{
	const swVector3 Col0(b.r[0][0], b.r[1][0], b.r[2][0]);
	const swVector3 Col1(b.r[0][1], b.r[1][1], b.r[2][1]);
	const swVector3 Col2(b.r[0][2], b.r[1][2], b.r[2][2]);

	const swVector3 Ret0(swDot(a.r[0], Col0), swDot(a.r[0], Col1), swDot(a.r[0], Col2));
	const swVector3 Ret1(swDot(a.r[1], Col0), swDot(a.r[1], Col1), swDot(a.r[1], Col2));
	const swVector3 Ret2(swDot(a.r[2], Col0), swDot(a.r[2], Col1), swDot(a.r[2], Col2));

	return swMatrix33(Ret0, Ret1, Ret2);
}

inline swMatrix44 swMul(const swMatrix44& a, const swMatrix44& b)
{
	swMatrix44 Result;

	Result.r[0] = b.r[0] * a[0].x + b.r[1] * a[0].y + b.r[2] * a[0].z + b.r[3] * a[0].w;
	Result.r[1] = b.r[0] * a[1].x + b.r[1] * a[1].y + b.r[2] * a[1].z + b.r[3] * a[1].w;
	Result.r[2] = b.r[0] * a[2].x + b.r[1] * a[2].y + b.r[2] * a[2].z + b.r[3] * a[2].w;
	Result.r[3] = b.r[0] * a[3].x + b.r[1] * a[3].y + b.r[2] * a[3].z + b.r[3] * a[3].w;

	return Result;
}

inline swMatrix33::swMatrix33(const swMatrix44& Matrix)
{
	r[0] = swVector3(Matrix.r[0].x, Matrix.r[0].y, Matrix.r[0].z);
	r[1] = swVector3(Matrix.r[1].x, Matrix.r[1].y, Matrix.r[1].z);
	r[2] = swVector3(Matrix.r[2].x, Matrix.r[2].y, Matrix.r[2].z);
}

inline void swMatrix33::Orthonormalize()
{
	r[0] = swNormalize(r[0]);
	r[1] = swNormalize(r[1] - swDot(r[1], r[0]) * r[0]);
	r[2] = r[2] - swDot(r[2], r[0]) * r[0];
	r[2] -= swDot(r[2], r[1]) * r[1];
	r[2] = swNormalize(r[2]);
}

inline swMatrix33 swMatrix33Identity()
{
	swMatrix33 m;

	m.r[0] = swVector3(1.0f, 0.0f, 0.0f);
	m.r[1] = swVector3(0.0f, 1.0f, 0.0f);
	m.r[2] = swVector3(0.0f, 0.0f, 1.0f);

	return m;
}

inline swMatrix33 swMatrix33Scale(const swVector3& Scale)
{
	swMatrix33 m;

	m.r[0] = swVector3(Scale.x, 0.0f, 0.0f);
	m.r[1] = swVector3(0.0f, Scale.y, 0.0f);
	m.r[2] = swVector3(0.0f, 0.0f, Scale.z);

	return m;
}

inline swMatrix33 swMatrix33RotationX(const float Radians)
{
	float s, c;

	s = sinf(Radians);
	c = cosf(Radians);

	swMatrix33 m;

	m.r[0] = swVector3(1.0f, 0.0f, 0.0f);
	m.r[1] = swVector3(0.0f,    c,    s);
	m.r[2] = swVector3(0.0f,   -s,    c);

	return m;
}

inline swMatrix33 swMatrix33RotationY(const float Radians)
{
	float s, c;

	s = sinf(Radians);
	c = cosf(Radians);

	swMatrix33 m;

	m.r[0] = swVector3(   c, 0.0f,   -s);
	m.r[1] = swVector3(0.0f, 1.0f, 0.0f);
	m.r[2] = swVector3(   s, 0.0f,    c);

	return m;
}

inline swMatrix33 swMatrix33RotationZ(const float Radians)
{
	float s = sinf(Radians);
	float c = cosf(Radians);

	swMatrix33 m;

	m.r[0] = swVector3(   c,    s, 0.0f);
	m.r[1] = swVector3(  -s,    c, 0.0f);
	m.r[2] = swVector3(0.0f, 0.0f, 1.0f);

	return m;
}

inline swMatrix33 swMatrix33FromAxisAngle(const swVector3& Axis, const float Radians)
{
	float s = sinf(Radians);
	float c = cosf(Radians);
	float mag = Axis.Length();

	if (mag == 0.0f)
		return swMatrix33Identity();

	swVector3 Normal = Axis * (1.0f / mag);

	float xx = Normal[0] * Normal[0];
	float yy = Normal[1] * Normal[1];
	float zz = Normal[2] * Normal[2];
	float xy = Normal[0] * Normal[1];
	float yz = Normal[1] * Normal[2];
	float zx = Normal[2] * Normal[0];
	float xs = Normal[0] * s;
	float ys = Normal[1] * s;
	float zs = Normal[2] * s;
	float one_c = 1.0f - c;

	swMatrix33 m;

	m.r[0] = swVector3((one_c * xx) + c, (one_c * xy) + zs, (one_c * zx) - ys);
	m.r[1] = swVector3((one_c * xy) - zs, (one_c * yy) + c, (one_c * yz) + xs);
	m.r[2] = swVector3((one_c * zx) + ys, (one_c * yz) - xs, (one_c * zz) + c);

	return m;
}

inline swMatrix33 swMatrix33Transpose(const swMatrix33& m)
{
	swMatrix33 t;

	t.r[0] = swVector3(m[0][0], m[1][0], m[2][0]);
	t.r[1] = swVector3(m[0][1], m[1][1], m[2][1]);
	t.r[2] = swVector3(m[0][2], m[1][2], m[2][2]);

	return t;
}

inline swMatrix33 swMatrix33AffineInverse(const swMatrix33& m)
{
	swMatrix33 Inv;

	Inv.r[0] = swVector3(m.r[0][0], m.r[1][0], m.r[2][0]);
	Inv.r[1] = swVector3(m.r[0][1], m.r[1][1], m.r[2][1]);
	Inv.r[2] = swVector3(m.r[0][2], m.r[1][2], m.r[2][2]);

	return Inv;
}

inline swMatrix33 swMatrix33FromEulerAngles(const swVector3& Radians)
{
	float CosYaw, SinYaw, CosPitch, SinPitch, CosRoll, SinRoll;

	CosRoll = cosf(Radians[0]);
	SinRoll = sinf(Radians[0]);
	CosPitch = cosf(Radians[1]);
	SinPitch = sinf(Radians[1]);
	CosYaw = cosf(Radians[2]);
	SinYaw = sinf(Radians[2]);

	swMatrix33 m;

	m.r[0] = swVector3(CosYaw * CosPitch, SinYaw * CosPitch, -SinPitch);
	m.r[1] = swVector3(CosYaw * SinPitch * SinRoll - SinYaw * CosRoll, CosYaw * CosRoll + SinYaw * SinPitch * SinRoll, CosPitch * SinRoll);
	m.r[2] = swVector3(CosYaw * SinPitch * CosRoll + SinYaw * SinRoll, SinYaw * SinPitch * CosRoll - CosYaw * SinRoll, CosPitch * CosRoll);

	return m;
}

inline swVector3 swMatrix33ToEulerAngles(const swMatrix33& RotMat)
{
	float SinPitch, CosPitch, SinRoll, CosRoll, SinYaw, CosYaw;

	SinPitch = -RotMat.r[0][2];
	CosPitch = sqrtf(1 - SinPitch*SinPitch);

	if (fabsf(CosPitch) > 0.0005f)
	{
		SinRoll = RotMat.r[1][2] / CosPitch;
		CosRoll = RotMat.r[2][2] / CosPitch;
		SinYaw = RotMat.r[0][1] / CosPitch;
		CosYaw = RotMat.r[0][0] / CosPitch;
	}
	else
	{
		SinRoll = -RotMat.r[2][1];
		CosRoll = RotMat.r[1][1];
		SinYaw = 0.0f;
		CosYaw = 1.0f;
	}

	swVector3 Rot(atan2f(SinRoll, CosRoll), atan2f(SinPitch, CosPitch), atan2f(SinYaw, CosYaw));

	if (Rot[0] < 0) Rot[0] += SW_2PI;
	if (Rot[1] < 0) Rot[1] += SW_2PI;
	if (Rot[2] < 0) Rot[2] += SW_2PI;

	return Rot;
}

inline bool swMatrix33Similar(const swMatrix33& a, const swMatrix33& b)
{
	for (int Row = 0; Row < 3; Row++)
	{
		float Dot = swDot(a.r[Row], b.r[Row]);

		if (Dot < 0.9999f || Dot > 1.0001f)
			return false;
	}

	return true;
}

inline float swMatrix44::Determinant() const
{
	return r[0][0] * r[1][1] * r[2][2] + r[0][1] * r[1][2] * r[2][0] +
	       r[0][2] * r[1][0] * r[2][1] - r[0][0] * r[1][2] * r[2][1] -
	       r[0][1] * r[1][0] * r[2][2] - r[0][2] * r[1][1] * r[2][0];
}

inline swMatrix44 swMatrix44Identity()
{
	swMatrix44 m;

	m.r[0] = swVector4(1.0f, 0.0f, 0.0f, 0.0f);
	m.r[1] = swVector4(0.0f, 1.0f, 0.0f, 0.0f);
	m.r[2] = swVector4(0.0f, 0.0f, 1.0f, 0.0f);
	m.r[3] = swVector4(0.0f, 0.0f, 0.0f, 1.0f);

	return m;
}

inline swMatrix44 swMatrix44Translation(const swVector3& Translation)
{
	swMatrix44 m;

	m.r[0] = swVector4(1.0f, 0.0f, 0.0f, 0.0f);
	m.r[1] = swVector4(0.0f, 1.0f, 0.0f, 0.0f);
	m.r[2] = swVector4(0.0f, 0.0f, 1.0f, 0.0f);
	m.r[3] = swVector4(Translation[0], Translation[1], Translation[2], 1.0f);

	return m;
}

inline swMatrix44 swMatrix44RotationX(const float Radians)
{
	float s, c;

	s = sinf(Radians);
	c = cosf(Radians);

	swMatrix44 m;

	m.r[0] = swVector4(1.0f, 0.0f, 0.0f, 0.0f);
	m.r[1] = swVector4(0.0f,    c,    s, 0.0f);
	m.r[2] = swVector4(0.0f,   -s,    c, 0.0f);
	m.r[3] = swVector4(0.0f, 0.0f, 0.0f, 1.0f);

	return m;
}

inline swMatrix44 swMatrix44RotationY(const float Radians)
{
	float s, c;

	s = sinf(Radians);
	c = cosf(Radians);

	swMatrix44 m;

	m.r[0] = swVector4(   c, 0.0f,   -s, 0.0f);
	m.r[1] = swVector4(0.0f, 1.0f, 0.0f, 0.0f);
	m.r[2] = swVector4(   s, 0.0f,    c, 0.0f);
	m.r[3] = swVector4(0.0f, 0.0f, 0.0f, 1.0f);

	return m;
}

inline swMatrix44 swMatrix44RotationZ(const float Radians)
{
	float s, c;

	s = sinf(Radians);
	c = cosf(Radians);

	swMatrix44 m;

	m.r[0] = swVector4(   c,    s, 0.0f, 0.0f);
	m.r[1] = swVector4(  -s,    c, 0.0f, 0.0f);
	m.r[2] = swVector4(0.0f, 0.0f, 1.0f, 0.0f);
	m.r[3] = swVector4(0.0f, 0.0f, 0.0f, 1.0f);

	return m;
}

inline swMatrix44 swMatrix44Scale(const swVector3& Scale)
{
	swMatrix44 m;

	m.r[0] = swVector4(Scale.x, 0.0f, 0.0f, 0.0f);
	m.r[1] = swVector4(0.0f, Scale.y, 0.0f, 0.0f);
	m.r[2] = swVector4(0.0f, 0.0f, Scale.z, 0.0f);
	m.r[3] = swVector4(0.0f, 0.0f, 0.0f, 1.0f);

	return m;
}

inline swMatrix44 swMatrix44LookAt(const swVector3& Eye, const swVector3& Target, const swVector3& Up)
{
	swVector3 x, y, z;

	z = swNormalize(Eye - Target);
	x = swNormalize(swCross(Up, z));
	y = swNormalize(swCross(z, x));

	swMatrix44 m;

	m.r[0] = swVector4(x[0], y[0], z[0], 0.0f);
	m.r[1] = swVector4(x[1], y[1], z[1], 0.0f);
	m.r[2] = swVector4(x[2], y[2], z[2], 0.0f);
	m.r[3] = m.r[0] * -Eye[0] + m.r[1] * -Eye[1] + m.r[2] * -Eye[2];
	m.r[3][3] = 1.0f;

	return m;
}

inline swMatrix44 swMatrix44Frustum(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
	if ((Near <= 0.0f) || (Far <= 0.0f) || (Near == Far) || (Left == Right) || (Top == Bottom))
		return swMatrix44Identity();

	float x, y, a, b, c, d;

	x = (2.0f * Near) / (Right - Left);
	y = (2.0f * Near) / (Top - Bottom);
	a = (Right + Left) / (Right - Left);
	b = (Top + Bottom) / (Top - Bottom);
	c = -(Far + Near) / (Far - Near);
	d = -(2.0f * Far * Near) / (Far - Near);

	swMatrix44 m;

	m.r[0] = swVector4(x, 0, 0, 0);
	m.r[1] = swVector4(0, y, 0, 0);
	m.r[2] = swVector4(a, b, c, -1);
	m.r[3] = swVector4(0, 0, d, 0);

	return m;
}

inline swMatrix44 swMatrix44Perspective(float FoVy, float Aspect, float Near, float Far)
{
	float Left, Right, Bottom, Top;

	Top = Near * (float)tan(FoVy * SW_PI / 360.0f);
	Bottom = -Top;

	Left = Bottom * Aspect;
	Right = Top * Aspect;

	return swMatrix44Frustum(Left, Right, Bottom, Top, Near, Far);
}

inline swMatrix44 swMatrix44Ortho(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
	swMatrix44 m;

	m.r[0] = swVector4(2.0f / (Right-Left), 0.0f, 0.0f, 0.0f),
	m.r[1] = swVector4(0.0f, 2.0f / (Top-Bottom), 0.0f, 0.0f),
	m.r[2] = swVector4(0.0f, 0.0f, -2.0f / (Far-Near), 0.0f),
	m.r[3] = swVector4(-(Right+Left) / (Right-Left), -(Top+Bottom) / (Top-Bottom), -(Far+Near) / (Far-Near), 1.0f);

	return m;
}

inline swMatrix44 swMatrix44FromAxisAngle(const swVector3& Axis, const float Radians)
{
	float s, c, mag, xx, yy, zz, xy, yz, zx, xs, ys, zs, one_c;

	s = sinf(Radians);
	c = cosf(Radians);
	mag = Axis.Length();

	if (mag == 0.0f)
		return swMatrix44Identity();

	swVector3 Normal = Axis * (1.0f / mag);

	xx = Normal[0] * Normal[0];
	yy = Normal[1] * Normal[1];
	zz = Normal[2] * Normal[2];
	xy = Normal[0] * Normal[1];
	yz = Normal[1] * Normal[2];
	zx = Normal[2] * Normal[0];
	xs = Normal[0] * s;
	ys = Normal[1] * s;
	zs = Normal[2] * s;
	one_c = 1.0f - c;

	swMatrix44 m;

	m.r[0] = swVector4((one_c * xx) + c, (one_c * xy) + zs, (one_c * zx) - ys, 0.0f);
	m.r[1] = swVector4((one_c * xy) - zs, (one_c * yy) + c, (one_c * yz) + xs, 0.0f);
	m.r[2] = swVector4((one_c * zx) + ys, (one_c * yz) - xs, (one_c * zz) + c, 0.0f);
	m.r[3] = swVector4(0.0f, 0.0f, 0.0f, 1.0f);

	return m;
}

inline swVector4 swMatrix44ToAxisAngle(const swMatrix44& m)
{
	swVector3 Rows[3];
	Rows[0] = swNormalize(swVector3(m.r[0][0], m.r[0][1], m.r[0][2]));
	Rows[1] = swNormalize(swVector3(m.r[1][0], m.r[1][1], m.r[1][2]));
	Rows[2] = swNormalize(swVector3(m.r[2][0], m.r[2][1], m.r[2][2]));

	if (m.Determinant() < 0.0f)
		Rows[0] *= -1.0f;

	const float Trace = Rows[0][0] + Rows[1][1] + Rows[2][2];
	const float Cos = 0.5f * (Trace - 1.0f);
	swVector4 rot;

	rot[3] = acosf(swClamp(Cos, -1.0f, 1.0f));  // in [0,PI]

	if (rot[3] > 0.01f)
	{
		if (fabsf(SW_PI - rot[3]) > 0.01f)
		{
			rot[0] = Rows[1][2] - Rows[2][1];
			rot[1] = Rows[2][0] - Rows[0][2];
			rot[2] = Rows[0][1] - Rows[1][0];

			float inv = 1.0f / sqrtf(rot[0]*rot[0] + rot[1]*rot[1] + rot[2]*rot[2]);

			rot[0] *= inv;
			rot[1] *= inv;
			rot[2] *= inv;
		}
		else
		{
			// angle is PI
			float HalfInverse;
			if (Rows[0][0] >= Rows[1][1])
			{
				// r00 >= r11
				if (Rows[0][0] >= Rows[2][2])
				{
					// r00 is maximum diagonal term
					rot[0] = 0.5f * sqrtf(Rows[0][0] - Rows[1][1] - Rows[2][2] + 1.0f);
					HalfInverse = 0.5f / rot[0];
					rot[1] = HalfInverse * Rows[1][0];
					rot[2] = HalfInverse * Rows[2][0];
				}
				else
				{
					// r22 is maximum diagonal term
					rot[2] = 0.5f * sqrtf(Rows[2][2] - Rows[0][0] - Rows[1][1] + 1.0f);
					HalfInverse = 0.5f / rot[2];
					rot[0] = HalfInverse * Rows[2][0];
					rot[1] = HalfInverse * Rows[2][1];
				}
			}
			else
			{
				// r11 > r00
				if (Rows[1][1] >= Rows[2][2])
				{
					// r11 is maximum diagonal term
					rot[1] = 0.5f * sqrtf(Rows[1][1] - Rows[0][0] - Rows[2][2] + 1.0f);
					HalfInverse  = 0.5f / rot[1];
					rot[0] = HalfInverse * Rows[1][0];
					rot[2] = HalfInverse * Rows[2][1];
				}
				else
				{
					// r22 is maximum diagonal term
					rot[2] = 0.5f * sqrtf(Rows[2][2] - Rows[0][0] - Rows[1][1] + 1.0f);
					HalfInverse = 0.5f / rot[2];
					rot[0] = HalfInverse * Rows[2][0];
					rot[1] = HalfInverse * Rows[2][1];
				}
			}
		}
	}
	else
	{
		// The angle is 0 and the matrix is the identity.
		rot[0] = 0.0f;
		rot[1] = 0.0f;
		rot[2] = 1.0f;
	}

	return rot;
}

inline swMatrix44 swMatrix44FromEulerAngles(const swVector3& Radians)
{
	float CosYaw, SinYaw, CosPitch, SinPitch, CosRoll, SinRoll;

	CosRoll = cosf(Radians[0]);
	SinRoll = sinf(Radians[0]);
	CosPitch = cosf(Radians[1]);
	SinPitch = sinf(Radians[1]);
	CosYaw = cosf(Radians[2]);
	SinYaw = sinf(Radians[2]);

	swMatrix44 m;

	m.r[0] = swVector4(CosYaw * CosPitch, SinYaw * CosPitch, -SinPitch, 0.0f);
	m.r[1] = swVector4(CosYaw * SinPitch * SinRoll - SinYaw * CosRoll, CosYaw * CosRoll + SinYaw * SinPitch * SinRoll, CosPitch * SinRoll, 0.0f);
	m.r[2] = swVector4(CosYaw * SinPitch * CosRoll + SinYaw * SinRoll, SinYaw * SinPitch * CosRoll - CosYaw * SinRoll, CosPitch * CosRoll, 0.0f);
	m.r[3] = swVector4(0.0f, 0.0f, 0.0f, 1.0f);

	return m;
}

inline swVector3 swMatrix44ToEulerAngles(const swMatrix44& RotMat)
{
	float SinPitch, CosPitch, SinRoll, CosRoll, SinYaw, CosYaw;

	SinPitch = -RotMat.r[0][2];
	CosPitch = sqrtf(1 - SinPitch*SinPitch);

	if (fabsf(CosPitch) > 0.0005f)
	{
		SinRoll = RotMat.r[1][2] / CosPitch;
		CosRoll = RotMat.r[2][2] / CosPitch;
		SinYaw = RotMat.r[0][1] / CosPitch;
		CosYaw = RotMat.r[0][0] / CosPitch;
	}
	else
	{
		SinRoll = -RotMat.r[2][1];
		CosRoll = RotMat.r[1][1];
		SinYaw = 0.0f;
		CosYaw = 1.0f;
	}

	swVector3 Rot(atan2f(SinRoll, CosRoll), atan2f(SinPitch, CosPitch), atan2f(SinYaw, CosYaw));

	if (Rot[0] < 0) Rot[0] += SW_2PI;
	if (Rot[1] < 0) Rot[1] += SW_2PI;
	if (Rot[2] < 0) Rot[2] += SW_2PI;

	return Rot;
}

inline swMatrix44 swMatrix44Transpose(const swMatrix44& m)
{
	swMatrix44 t;

	t.r[0] = swVector4(m[0][0], m[1][0], m[2][0], m[3][0]);
	t.r[1] = swVector4(m[0][1], m[1][1], m[2][1], m[3][1]);
	t.r[2] = swVector4(m[0][2], m[1][2], m[2][2], m[3][2]);
	t.r[3] = swVector4(m[0][3], m[1][3], m[2][3], m[3][3]);

	return t;
}

inline swMatrix44 swMatrix44AffineInverse(const swMatrix44& m)
{
	swMatrix44 Inv;

	Inv.r[0] = swVector4(m.r[0][0], m.r[1][0], m.r[2][0], m.r[0][3]);
	Inv.r[1] = swVector4(m.r[0][1], m.r[1][1], m.r[2][1], m.r[1][3]);
	Inv.r[2] = swVector4(m.r[0][2], m.r[1][2], m.r[2][2], m.r[2][3]);

	swVector3 Trans = -swMul30(m.r[3], Inv);
	Inv.r[3] = swVector4(Trans[0], Trans[1], Trans[2], 1.0f);

	return Inv;
}

// Inverse code from the GLU library.
inline swMatrix44 swMatrix44Inverse(const swMatrix44& m)
{
#define SWAP_ROWS(a, b) { float *_tmp = a; (a)=(b); (b)=_tmp; }
#define MAT(m,col,row) m.r[row][col]

	float wtmp[4][8];
	float m0, m1, m2, m3, s;
	float *r0, *r1, *r2, *r3;

	r0 = wtmp[0], r1 = wtmp[1], r2 = wtmp[2], r3 = wtmp[3];

	r0[0] = MAT(m,0,0), r0[1] = MAT(m,0,1),
	r0[2] = MAT(m,0,2), r0[3] = MAT(m,0,3),
	r0[4] = 1.0, r0[5] = r0[6] = r0[7] = 0.0,

	r1[0] = MAT(m,1,0), r1[1] = MAT(m,1,1),
	r1[2] = MAT(m,1,2), r1[3] = MAT(m,1,3),
	r1[5] = 1.0, r1[4] = r1[6] = r1[7] = 0.0,

	r2[0] = MAT(m,2,0), r2[1] = MAT(m,2,1),
	r2[2] = MAT(m,2,2), r2[3] = MAT(m,2,3),
	r2[6] = 1.0, r2[4] = r2[5] = r2[7] = 0.0,

	r3[0] = MAT(m,3,0), r3[1] = MAT(m,3,1),
	r3[2] = MAT(m,3,2), r3[3] = MAT(m,3,3),
	r3[7] = 1.0, r3[4] = r3[5] = r3[6] = 0.0;

	// choose pivot - or die
	if (fabs(r3[0])>fabs(r2[0])) SWAP_ROWS(r3, r2);
	if (fabs(r2[0])>fabs(r1[0])) SWAP_ROWS(r2, r1);
	if (fabs(r1[0])>fabs(r0[0])) SWAP_ROWS(r1, r0);
//	if (0.0 == r0[0])  return GL_FALSE;

	// eliminate first variable
	m1 = r1[0]/r0[0]; m2 = r2[0]/r0[0]; m3 = r3[0]/r0[0];
	s = r0[1]; r1[1] -= m1 * s; r2[1] -= m2 * s; r3[1] -= m3 * s;
	s = r0[2]; r1[2] -= m1 * s; r2[2] -= m2 * s; r3[2] -= m3 * s;
	s = r0[3]; r1[3] -= m1 * s; r2[3] -= m2 * s; r3[3] -= m3 * s;
	s = r0[4];
	if (s != 0.0) { r1[4] -= m1 * s; r2[4] -= m2 * s; r3[4] -= m3 * s; }
	s = r0[5];
	if (s != 0.0) { r1[5] -= m1 * s; r2[5] -= m2 * s; r3[5] -= m3 * s; }
	s = r0[6];
	if (s != 0.0) { r1[6] -= m1 * s; r2[6] -= m2 * s; r3[6] -= m3 * s; }
	s = r0[7];
	if (s != 0.0) { r1[7] -= m1 * s; r2[7] -= m2 * s; r3[7] -= m3 * s; }

	// choose pivot - or die
	if (fabs(r3[1])>fabs(r2[1])) SWAP_ROWS(r3, r2);
	if (fabs(r2[1])>fabs(r1[1])) SWAP_ROWS(r2, r1);
//	if (0.0 == r1[1])  return GL_FALSE;

	// eliminate second variable
	m2 = r2[1]/r1[1]; m3 = r3[1]/r1[1];
	r2[2] -= m2 * r1[2]; r3[2] -= m3 * r1[2];
	r2[3] -= m2 * r1[3]; r3[3] -= m3 * r1[3];
	s = r1[4]; if (0.0 != s) { r2[4] -= m2 * s; r3[4] -= m3 * s; }
	s = r1[5]; if (0.0 != s) { r2[5] -= m2 * s; r3[5] -= m3 * s; }
	s = r1[6]; if (0.0 != s) { r2[6] -= m2 * s; r3[6] -= m3 * s; }
	s = r1[7]; if (0.0 != s) { r2[7] -= m2 * s; r3[7] -= m3 * s; }

	// choose pivot - or die
	if (fabs(r3[2])>fabs(r2[2])) SWAP_ROWS(r3, r2);
//	if (0.0 == r2[2])  return GL_FALSE;

	// eliminate third variable
	m3 = r3[2]/r2[2];
	r3[3] -= m3 * r2[3], r3[4] -= m3 * r2[4],
	r3[5] -= m3 * r2[5], r3[6] -= m3 * r2[6],
	r3[7] -= m3 * r2[7];

	// last check
//	if (0.0 == r3[3]) return GL_FALSE;

	s = 1.0f/r3[3];              // now back substitute row 3
	r3[4] *= s; r3[5] *= s; r3[6] *= s; r3[7] *= s;

	m2 = r2[3];                 // now back substitute row 2
	s  = 1.0f/r2[2];
	r2[4] = s * (r2[4] - r3[4] * m2), r2[5] = s * (r2[5] - r3[5] * m2),
	r2[6] = s * (r2[6] - r3[6] * m2), r2[7] = s * (r2[7] - r3[7] * m2);
	m1 = r1[3];
	r1[4] -= r3[4] * m1, r1[5] -= r3[5] * m1,
	r1[6] -= r3[6] * m1, r1[7] -= r3[7] * m1;
	m0 = r0[3];
	r0[4] -= r3[4] * m0, r0[5] -= r3[5] * m0,
	r0[6] -= r3[6] * m0, r0[7] -= r3[7] * m0;

	m1 = r1[2];                 // now back substitute row 1
	s  = 1.0f/r1[1];
	r1[4] = s * (r1[4] - r2[4] * m1), r1[5] = s * (r1[5] - r2[5] * m1),
	r1[6] = s * (r1[6] - r2[6] * m1), r1[7] = s * (r1[7] - r2[7] * m1);
	m0 = r0[2];
	r0[4] -= r2[4] * m0, r0[5] -= r2[5] * m0,
	r0[6] -= r2[6] * m0, r0[7] -= r2[7] * m0;

	m0 = r0[1];                 // now back substitute row 0
	s  = 1.0f/r0[0];
	r0[4] = s * (r0[4] - r1[4] * m0), r0[5] = s * (r0[5] - r1[5] * m0),
	r0[6] = s * (r0[6] - r1[6] * m0), r0[7] = s * (r0[7] - r1[7] * m0);

	const swVector4 Row0(r0[4], r1[4], r2[4], r3[4]);
	const swVector4 Row1(r0[5], r1[5], r2[5], r3[5]);
	const swVector4 Row2(r0[6], r1[6], r2[6], r3[6]);
	const swVector4 Row3(r0[7], r1[7], r2[7], r3[7]);

	swMatrix44 out(Row0, Row1, Row2, Row3);

	return out;

#undef MAT
#undef SWAP_ROWS
}

inline swMatrix44 swMatrix44StudWorksToLDraw(const swMatrix44& Matrix)
{
	swMatrix44 m;

	m.r[0] = swVector4(Matrix[0][0], -Matrix[2][0], Matrix[1][0], 0.0f);
	m.r[1] = swVector4(-Matrix[0][2], Matrix[2][2], -Matrix[1][2], 0.0f);
	m.r[2] = swVector4(Matrix[0][1], -Matrix[2][1], Matrix[1][1], 0.0f);
	m.r[3] = swVector4(Matrix[3][0], -Matrix[3][2], Matrix[3][1], 1.0f);

	return m;
}

inline swMatrix44 swMatrix44LDrawToStudWorks(const swMatrix44& Matrix)
{
	swMatrix44 m;

	m.r[0] = swVector4(Matrix[0][0], Matrix[0][2], -Matrix[0][1], 0.0f);
	m.r[1] = swVector4(Matrix[2][0], Matrix[2][2], -Matrix[2][1], 0.0f);
	m.r[2] = swVector4(-Matrix[1][0], -Matrix[1][2], Matrix[1][1], 0.0f);
	m.r[3] = swVector4(Matrix[3][0], Matrix[3][2], -Matrix[3][1], 1.0f);

	return m;
}

inline swVector4 swQuaternionRotationX(float Radians)
{
	return swVector4(sinf(Radians / 2.0f), 0, 0, cosf(Radians / 2.0f));
}

inline swVector4 swQuaternionRotationY(float Radians)
{
	return swVector4(0, sinf(Radians / 2.0f), 0, cosf(Radians / 2.0f));
}

inline swVector4 swQuaternionRotationZ(float Radians)
{
	return swVector4(0, 0, sinf(Radians / 2.0f), cosf(Radians / 2.0f));
}

inline swVector4 swQuaternionFromAxisAngle(const swVector4& a)
{
	const float s = sinf(a[3] / 2.0f);
	return swVector4(a[0] * s, a[1] * s, a[2] * s, cosf(a[3] / 2.0f));
}

inline swVector4 swQuaternionToAxisAngle(const swVector4& a)
{
	const float Len = swDot3(a, a);

	if (Len > 0.00001f)
	{
		const float f = 1.0f / sqrtf(Len);
		return swVector4(a[0] * f, a[1] * f, a[2] * f, acosf(a[3]) * 2.0f);
	}
	else
	{
		return swVector4(0, 0, 1, 0);
	}
}

inline swVector4 swQuaternionMultiply(const swVector4& a, const swVector4& b)
{
	const float x =  a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
	const float y = -a[0] * b[2] + a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
	const float z =  a[0] * b[1] - a[1] * b[0] + a[2] * b[3] + a[3] * b[2];
	const float w = -a[0] * b[0] - a[1] * b[1] - a[2] * b[2] + a[3] * b[3];

	return swVector4(x, y, z, w);
}

inline swVector3 swQuaternionMul(const swVector3& a, const swVector4& b)
{
	// Faster to transform to a matrix and multiply.
	const float Tx  = 2.0f*b[0];
	const float Ty  = 2.0f*b[1];
	const float Tz  = 2.0f*b[2];
	const float Twx = Tx*b[3];
	const float Twy = Ty*b[3];
	const float Twz = Tz*b[3];
	const float Txx = Tx*b[0];
	const float Txy = Ty*b[0];
	const float Txz = Tz*b[0];
	const float Tyy = Ty*b[1];
	const float Tyz = Tz*b[1];
	const float Tzz = Tz*b[2];

	swVector3 Rows[3];
	Rows[0] = swVector3(1.0f-(Tyy+Tzz), Txy+Twz, Txz-Twy);
	Rows[1] = swVector3(Txy-Twz, 1.0f-(Txx+Tzz), Tyz+Twx);
	Rows[2] = swVector3(Txz+Twy, Tyz-Twx, 1.0f-(Txx+Tyy));

	return swVector3(Rows[0]*a[0] + Rows[1]*a[1] + Rows[2]*a[2]);
}

// Convert world coordinates to screen coordinates.
inline swVector3 swProjectPoint(const swVector3& Point, const swMatrix44& ModelView, const swMatrix44& Projection, const int (&Viewport)[4])
{
	swVector4 Tmp;

	Tmp = swMul4(swVector4(Point[0], Point[1], Point[2], 1.0f), ModelView);
	Tmp = swMul4(Tmp, Projection);

	// Normalize.
	Tmp /= Tmp[3];

	// Screen coordinates.
	return swVector3(Viewport[0] + (1 + Tmp[0]) * Viewport[2] / 2, Viewport[1] + (1 + Tmp[1]) * Viewport[3] / 2, (1 + Tmp[2]) / 2);
}

inline swVector3 swUnprojectPoint(const swVector3& Point, const swMatrix44& ModelView, const swMatrix44& Projection, const int (&Viewport)[4])
{
	// Calculate the screen to model transform.
	const swMatrix44 Transform = swMatrix44Inverse(swMul(ModelView, Projection));

	swVector4 Tmp;

	// Convert the point to homogeneous coordinates.
	Tmp[0] = (Point[0] - Viewport[0]) * 2.0f / Viewport[2] - 1.0f;
	Tmp[1] = (Point[1] - Viewport[1]) * 2.0f / Viewport[3] - 1.0f;
	Tmp[2] = Point[2] * 2.0f - 1.0f;
	Tmp[3] = 1.0f;

	Tmp = swMul4(Tmp, Transform);

	if (Tmp[3] != 0.0f)
		Tmp /= Tmp[3];

	return swVector3(Tmp[0], Tmp[1], Tmp[2]);
}

inline void swUnprojectPoints(swVector3* Points, int NumPoints, const swMatrix44& ModelView, const swMatrix44& Projection, const int (&Viewport)[4])
{
	// Calculate the screen to model transform.
	const swMatrix44 Transform = swMatrix44Inverse(swMul(ModelView, Projection));

	for (int i = 0; i < NumPoints; i++)
	{
		swVector4 Tmp;

		// Convert the point to homogeneous coordinates.
		Tmp[0] = (Points[i][0] - Viewport[0]) * 2.0f / Viewport[2] - 1.0f;
		Tmp[1] = (Points[i][1] - Viewport[1]) * 2.0f / Viewport[3] - 1.0f;
		Tmp[2] = Points[i][2] * 2.0f - 1.0f;
		Tmp[3] = 1.0f;

		Tmp = swMul4(Tmp, Transform);

		if (Tmp[3] != 0.0f)
			Tmp /= Tmp[3];

		Points[i] = swVector3(Tmp[0], Tmp[1], Tmp[2]);
	}
}

inline void swGetFrustumPlanes(const swMatrix44& WorldView, const swMatrix44& Projection, swVector4 (&Planes)[6])
{
	swMatrix44 WorldProj = swMul(WorldView, Projection);

	Planes[0][0] = (WorldProj[0][0] - WorldProj[0][3]) * -1;
	Planes[0][1] = (WorldProj[1][0] - WorldProj[1][3]) * -1;
	Planes[0][2] = (WorldProj[2][0] - WorldProj[2][3]) * -1;
	Planes[0][3] = (WorldProj[3][0] - WorldProj[3][3]) * -1;
	Planes[1][0] =  WorldProj[0][0] + WorldProj[0][3];
	Planes[1][1] =  WorldProj[1][0] + WorldProj[1][3];
	Planes[1][2] =  WorldProj[2][0] + WorldProj[2][3];
	Planes[1][3] =  WorldProj[3][0] + WorldProj[3][3];
	Planes[2][0] = (WorldProj[0][1] - WorldProj[0][3]) * -1;
	Planes[2][1] = (WorldProj[1][1] - WorldProj[1][3]) * -1;
	Planes[2][2] = (WorldProj[2][1] - WorldProj[2][3]) * -1;
	Planes[2][3] = (WorldProj[3][1] - WorldProj[3][3]) * -1;
	Planes[3][0] =  WorldProj[0][1] + WorldProj[0][3];
	Planes[3][1] =  WorldProj[1][1] + WorldProj[1][3];
	Planes[3][2] =  WorldProj[2][1] + WorldProj[2][3];
	Planes[3][3] =  WorldProj[3][1] + WorldProj[3][3];
	Planes[4][0] = (WorldProj[0][2] - WorldProj[0][3]) * -1;
	Planes[4][1] = (WorldProj[1][2] - WorldProj[1][3]) * -1;
	Planes[4][2] = (WorldProj[2][2] - WorldProj[2][3]) * -1;
	Planes[4][3] = (WorldProj[3][2] - WorldProj[3][3]) * -1;
	Planes[5][0] =  WorldProj[0][2] + WorldProj[0][3];
	Planes[5][1] =  WorldProj[1][2] + WorldProj[1][3];
	Planes[5][2] =  WorldProj[2][2] + WorldProj[2][3];
	Planes[5][3] =  WorldProj[3][2] + WorldProj[3][3];

	for (int i = 0; i < 6; i++)
	{
		const swVector3 Normal(Planes[i][0], Planes[i][1], Planes[i][2]);
		const float Length = Normal.Length();
		Planes[i] /= -Length;
	}
}

inline std::tuple<swVector3, float> swZoomExtents(const swVector3& Position, const swMatrix44& WorldView, const swMatrix44& Projection, const swVector3* Points, size_t NumPoints)
{
	if (!NumPoints)
		return std::make_tuple(Position, 2500.0f);

	swVector4 Planes[6];
	swGetFrustumPlanes(WorldView, Projection, Planes);

	const swVector3 Front(WorldView[0][2], WorldView[1][2], WorldView[2][2]);

	float SmallestDistance = FLT_MAX;

	for (int PlaneIdx = 0; PlaneIdx < 4; PlaneIdx++)
	{
		const swVector3 Plane(Planes[PlaneIdx][0], Planes[PlaneIdx][1], Planes[PlaneIdx][2]);
		const float ep = swDot(Position, Plane);
		const float fp = swDot(Front, Plane);

		for (size_t PointIdx = 0; PointIdx < NumPoints; PointIdx++)
		{
			const float u = (ep - swDot(Points[PointIdx], Plane)) / fp;

			if (u < SmallestDistance)
				SmallestDistance = u;
		}
	}

	swVector3 NewPosition = Position - (Front * SmallestDistance);

	float FarDistance = 2500.0f;

	for (size_t PointIdx = 0; PointIdx < NumPoints; PointIdx++)
	{
		const float Distance = swDot(Points[PointIdx], Front);

		if (Distance > FarDistance)
			FarDistance = Distance;
	}

	return std::make_tuple(NewPosition, FarDistance + swDot(NewPosition, Front));
}

inline void swClosestPointsBetweenLines(const swVector3& Line1a, const swVector3& Line1b, const swVector3& Line2a, const swVector3& Line2b, swVector3* Intersection1, swVector3* Intersection2)
{
	const swVector3 u1 = Line1b - Line1a;
	const swVector3 u2 = Line2b - Line2a;
	const swVector3 p21 = Line2a - Line1a;
	const swVector3 m = swCross(u2, u1);
	const float m2 = swDot(m, m);

	if (m2 < 0.00001f)
	{
		if (Intersection1)
			*Intersection1 = Line1a;
		if (Intersection2)
			*Intersection2 = Line2a;
		return;
	}

	const swVector3 r = swCross(p21, m / m2);

	if (Intersection1)
	{
		const float t1 = swDot(r, u2);
		*Intersection1 = Line1a + t1 * u1;
	}

	if (Intersection2)
	{
		const float t2 = swDot(r, u1);
		*Intersection2 = Line2a + t2 * u2;
	}
}

inline bool swLineSegmentPlaneIntersection(swVector3* Intersection, const swVector3& Start, const swVector3& End, const swVector4& Plane)
{
	const swVector3 Dir = End - Start;
	const swVector3 PlaneNormal(Plane[0], Plane[1], Plane[2]);

	const float t1 = swDot(PlaneNormal, Start) + Plane[3];
	const float t2 = swDot(PlaneNormal, Dir);

	if (t2 == 0.0f)
		return false;

	const float t = -t1 / t2;

	*Intersection = Start + t * Dir;

	if ((t < 0.0f) || (t > 1.0f))
		return false;

	return true;
}

inline bool swLineTriangleMinIntersection(const swVector3& p1, const swVector3& p2, const swVector3& p3, const swVector3& Start, const swVector3& End, float* MinDist, swVector3* Intersection)
{
	// Calculate the polygon plane.
	const swVector3 PlaneNormal = swCross(p1 - p2, p3 - p2);
	const float PlaneD = -swDot(PlaneNormal, p1);

	// Check if the line is parallel to the plane.
	const swVector3 Dir = End - Start;

	const float t1 = swDot(PlaneNormal, Start) + PlaneD;
	const float t2 = swDot(PlaneNormal, Dir);

	if (t2 == 0)
		return false;

	const float t = -(t1 / t2);

	if (t < 0)
		return false;

	// Intersection of the plane and line segment.
	*Intersection = Start - (t1 / t2) * Dir;

	float Dist = swLength(Start - *Intersection);

	if (Dist > *MinDist)
		return false;

	// Check if we're inside the triangle.
	swVector3 pa1, pa2, pa3;
	pa1 = swNormalize(p1 - *Intersection);
	pa2 = swNormalize(p2 - *Intersection);
	pa3 = swNormalize(p3 - *Intersection);

	float a1, a2, a3;
	a1 = swDot(pa1, pa2);
	a2 = swDot(pa2, pa3);
	a3 = swDot(pa3, pa1);

	const float total = (acosf(a1) + acosf(a2) + acosf(a3)) * SW_RTOD;

	if (fabs(total - 360) <= 0.001f)
	{
		*MinDist = Dist;
		return true;
	}

	return false;
}

// Sutherland-Hodgman method of clipping a polygon to a plane.
inline void swPolygonPlaneClip(swVector3* InPoints, int NumInPoints, swVector3* OutPoints, int* NumOutPoints, const swVector4& Plane)
{
	swVector3 *s, *p, i;

	*NumOutPoints = 0;
	s = &InPoints[NumInPoints-1];

	for (int j = 0; j < NumInPoints; j++)
	{
		p = &InPoints[j];

		if (swDot3(*p, Plane) + Plane[3] <= 0)
		{
			if (swDot3(*s, Plane) + Plane[3] <= 0)
			{
				// Both points inside.
				OutPoints[*NumOutPoints] = *p;
				*NumOutPoints = *NumOutPoints + 1;
			}
			else
			{
				// Outside, inside.
				swLineSegmentPlaneIntersection(&i, *s, *p, Plane);

				OutPoints[*NumOutPoints] = i;
				*NumOutPoints = *NumOutPoints + 1;
				OutPoints[*NumOutPoints] = *p;
				*NumOutPoints = *NumOutPoints + 1;
			}
		}
		else
		{
			if (swDot3(*s, Plane) + Plane[3] <= 0)
			{
				// Inside, outside.
				swLineSegmentPlaneIntersection(&i, *s, *p, Plane);

				OutPoints[*NumOutPoints] = i;
				*NumOutPoints = *NumOutPoints + 1;
			}
		}

		s = p;
	}
}

// Return true if a polygon intersects a set of planes.
inline bool swTriangleIntersectsPlanes(const swVector3& p1, const swVector3& p2, const swVector3& p3, const swVector4 (&Planes)[6])
{
	constexpr int NumPlanes = 6;
	const swVector3 Points[3] = { p1, p2, p3 };
	int Outcodes[3] = { 0, 0, 0 }, i;
	constexpr int NumPoints = 3;

	// First do the Cohen-Sutherland out code test for trivial rejects/accepts.
	for (i = 0; i < NumPoints; i++)
	{
		const swVector3 Pt(Points[i][0], Points[i][1], Points[i][2]);

		for (int j = 0; j < NumPlanes; j++)
		{
			if (swDot3(Pt, Planes[j]) + Planes[j][3] > 0)
				Outcodes[i] |= 1 << j;
		}
	}

	// Polygon completely outside a plane.
	if ((Outcodes[0] & Outcodes[1] & Outcodes[2]) != 0)
		return false;

	// If any vertex has an out code of all zeros then we intersect the volume.
	if (!Outcodes[0] || !Outcodes[1] || !Outcodes[2])
		return true;

	// Buffers for clipping the polygon.
	swVector3 ClipPoints[2][8];
	int NumClipPoints[2];
	int ClipBuffer = 0;

	NumClipPoints[0] = NumPoints;
	ClipPoints[0][0] = swVector3(p1[0], p1[1], p1[2]);
	ClipPoints[0][1] = swVector3(p2[0], p2[1], p2[2]);
	ClipPoints[0][2] = swVector3(p3[0], p3[1], p3[2]);

	// Now clip the polygon against the planes.
	for (i = 0; i < NumPlanes; i++)
	{
		swPolygonPlaneClip(ClipPoints[ClipBuffer], NumClipPoints[ClipBuffer], ClipPoints[ClipBuffer^1], &NumClipPoints[ClipBuffer^1], Planes[i]);
		ClipBuffer ^= 1;

		if (!NumClipPoints[ClipBuffer])
			return false;
	}

	return true;
}

// Return true if a ray intersects a bounding box, and calculates the distance from the start of the ray (adapted from Graphics Gems).
inline bool swBoundingBoxRayIntersectDistance(const swVector3& Min, const swVector3& Max, const swVector3& Start, const swVector3& End, float* Dist, swVector3* Intersection, swVector3* Plane)
{
	bool MiddleQuadrant[3];
	bool Inside = true;
	float CandidatePlane[3];
	float MaxT[3];
	int i;

	// Find candidate planes.
	for (i = 0; i < 3; i++)
	{
		if (Start[i] < Min[i])
		{
			MiddleQuadrant[i] = false;
			CandidatePlane[i] = Min[i];
			Inside = false;
		}
		else if (Start[i] > Max[i])
		{
			MiddleQuadrant[i] = false;
			CandidatePlane[i] = Max[i];
			Inside = false;
		}
		else
		{
			MiddleQuadrant[i] = true;
			CandidatePlane[i] = 0.0f;
		}
	}

	// Ray origin inside box.
	if (Inside)
	{
		*Dist = 0;

		if (Intersection)
			*Intersection = Start;

		if (Plane)
			*Plane = Start;

		return true;
	}

	// Calculate T distances to candidate planes.
	swVector3 Dir = End - Start;

	for (i = 0; i < 3; i++)
	{
		if (!MiddleQuadrant[i] && Dir[i] != 0.0f)
			MaxT[i] = (CandidatePlane[i] - Start[i]) / Dir[i];
		else
			MaxT[i] = -1.0f;
	}

	// Get largest of the MaxT's for final choice of intersection.
	int WhichPlane = 0;
	for (i = 1; i < 3; i++)
		if (MaxT[WhichPlane] < MaxT[i])
			WhichPlane = i;

	// Check final candidate actually inside box.
	if (MaxT[WhichPlane] < 0.0f)
		return false;

	swVector3 Point;

	for (i = 0; i < 3; i++)
	{
		if (WhichPlane != i)
		{
			Point[i] = Start[i] + MaxT[WhichPlane] * Dir[i];
			if (Point[i] < Min[i] || Point[i] > Max[i])
				return false;
		}
		else
			Point[i] = CandidatePlane[i];
	}

	*Dist = swLength(Point - Start);

	if (Intersection)
		*Intersection = Point;

	if (Plane)
	{
		*Plane = swVector3(0.0f, 0.0f, 0.0f);
		(*Plane)[WhichPlane] = CandidatePlane[WhichPlane];
	}

	return true;
}

inline bool swSphereRayIntersection(const swVector3& Center, float Radius, const swVector3& Start, const swVector3& End, swVector3& Intersection)
{
	const swVector3 RayDirection = swNormalize(End - Start);
	const swVector3 RayCenter = Center - Start;
	const float RayCenterSquared = swDot(RayCenter, RayCenter);
	const float ClosestApproach = swDot(RayCenter, RayDirection);

	if (ClosestApproach < 0)
		return false;

	const float HalfCordSquared = (Radius * Radius) - RayCenterSquared + (ClosestApproach * ClosestApproach);

	if (HalfCordSquared < 0)
		return false;

	Intersection = Start + RayDirection * (ClosestApproach - sqrtf(HalfCordSquared));

	return true;
}

inline bool swSphereRayMinIntersectDistance(const swVector3& Center, float Radius, const swVector3& Start, const swVector3& End, float* Dist)
{
	const swVector3 Dir = Center - Start;
	const float LengthSquaredDir = swLengthSquared(Dir);
	const float RadiusSquared = Radius * Radius;

	if (LengthSquaredDir < RadiusSquared)
	{
		// Ray origin inside sphere.
		*Dist = 0;
		return true;
	}
	else
	{
		const swVector3 RayDir = End - Start;
		float t = swDot(Dir, RayDir) / swLengthSquared(RayDir);

		// Ray points away from sphere.
		if (t < 0)
			return false;

		const float c = (RadiusSquared - LengthSquaredDir) / swLengthSquared(RayDir) + (t * t);
		if (c > 0)
		{
			*Dist = t - sqrtf(c);
			return true;
		}

		return false;
	}
}

inline bool swConeRayMinIntersectDistance(const swVector3& Tip, const swVector3& Direction, float Radius, float Height, const swVector3& Start, const swVector3& End, float* Dist)
{
	const swVector3 v = End - Start;
	const swVector3 h = Direction;
	const swVector3 w = Start - Tip;
	const float vh = swDot(v, h);
	const float wh = swDot(w, h);
	const float m = (Radius * Radius) / (Height * Height);

	const float a = swDot(v, v) - m * vh * vh - vh * vh;
	const float b = 2 * (swDot(v, w) - m * vh * wh - vh * wh);
	const float c = swDot(w, w) - m * wh * wh - wh * wh;

	const float delta = b * b - 4 * a * c;

	if (delta < 0.0f)
		return false;

	float ts[2] = { (-b - sqrtf(delta)) / (2 * a), (-b + sqrtf(delta)) / (2 * a) };

	for (int ti = 0; ti < 2; ti++)
	{
		float t = ts[ti];
		swVector3 Intersection = Start + v * t;

		float ConeD = swDot(Intersection - Tip, Direction);

		if (ConeD < 0.0f)
			continue;

		if (ConeD < Height)
		{
			*Dist = swLength(Intersection - Start);

			return true;
		}

		swVector3 Center(Tip + Direction * Height);
		swVector4 Plane(Direction, -swDot(Direction, Center));

		if (swLineSegmentPlaneIntersection(&Intersection, Start, End, Plane))
		{
			if (swLengthSquared(Center - Intersection) < Radius * Radius)
			{
				*Dist = swLength(Intersection - Start);

				return true;
			}
		}
	}

	return false;
}

inline bool swCylinderRayMinIntersectDistance(float Radius, float Height, const swVector3& Start, const swVector3& End, float* Dist)
{
	swVector4 BottomPlane(0.0f, 0.0f, 1.0f, 0.0f);
	swVector3 Intersection;
	float MinDistance = FLT_MAX;

	if (swLineSegmentPlaneIntersection(&Intersection, Start, End, BottomPlane))
	{
		if (Intersection.x * Intersection.x + Intersection.y * Intersection.y < Radius * Radius)
		{
			float Distance = swLength(Intersection - Start);

			if (Distance < MinDistance)
				MinDistance = Distance;
		}
	}

	swVector4 TopPlane(0.0f, 0.0f, 1.0f, -Height);

	if (swLineSegmentPlaneIntersection(&Intersection, Start, End, TopPlane))
	{
		if (Intersection.x * Intersection.x + Intersection.y * Intersection.y < Radius * Radius)
		{
			float Distance = swLength(Intersection - Start);

			if (Distance < MinDistance)
				MinDistance = Distance;
		}
	}

	swVector3 Direction = End - Start;

	float a = (Direction.x * Direction.x) + (Direction.y * Direction.y);
	float b = 2 * (Direction.x * Start.x + Direction.y * Start.y);
	float c = (Start.x * Start.x) + (Start.y * Start.y) - (Radius * Radius);

	float delta = b * b - 4 * (a * c);

	if (delta > 0.0f)
	{
		float ts[2] = { (-b - sqrtf(delta)) / (2 * a), (-b + sqrtf(delta)) / (2 * a) };

		for (int ti = 0; ti < 2; ti++)
		{
			float t = ts[ti];
			Intersection = Start + Direction * t;

			if (Intersection.z < 0.0f || Intersection.z > Height)
				continue;

			float Distance = swLength(Intersection - Start);

			if (Distance < MinDistance)
				MinDistance = Distance;
		}
	}

	if (MinDistance == FLT_MAX)
		return false;

	*Dist = MinDistance;

	return true;
}

inline swVector3 swRayPointClosestPoint(const swVector3& Point, const swVector3& Start, const swVector3& End)
{
	const swVector3 Dir = Point - Start;
	const swVector3 RayDir = End - Start;

	float t = swDot(Dir, RayDir) / swLengthSquared(RayDir);
	t = swClamp(t, 0.0f, 1.0f);

	return Start + t * RayDir;
}

inline float swRayPointDistance(const swVector3& Point, const swVector3& Start, const swVector3& End)
{
	const swVector3 Closest = swRayPointClosestPoint(Point, Start, End);

	return swLength(Closest - Point);
}

// Returns true if the axis aligned box intersects the volume defined by planes.
inline bool swBoundingBoxIntersectsVolume(const swVector3& Min, const swVector3& Max, const swVector4 (&Planes)[6])
{
	constexpr int NumPlanes = 6;
	swVector3 Points[8] =
	{
		Points[0] = swVector3(Min[0], Min[1], Min[2]),
		Points[1] = swVector3(Min[0], Max[1], Min[2]),
		Points[2] = swVector3(Max[0], Max[1], Min[2]),
		Points[3] = swVector3(Max[0], Min[1], Min[2]),
		Points[4] = swVector3(Min[0], Min[1], Max[2]),
		Points[5] = swVector3(Min[0], Max[1], Max[2]),
		Points[6] = swVector3(Max[0], Max[1], Max[2]),
		Points[7] = swVector3(Max[0], Min[1], Max[2])
	};

	// Start by testing trivial reject/accept cases.
	int Outcodes[8];
	int i;

	for (i = 0; i < 8; i++)
	{
		Outcodes[i] = 0;

		for (int j = 0; j < NumPlanes; j++)
		{
			if (swDot3(Points[i], Planes[j]) + Planes[j][3] > 0)
				Outcodes[i] |= 1 << j;
		}
	}

	int OutcodesOR = 0, OutcodesAND = 0x3f;

	for (i = 0; i < 8; i++)
	{
		OutcodesAND &= Outcodes[i];
		OutcodesOR |= Outcodes[i];
	}

	// All corners outside the same plane.
	if (OutcodesAND != 0)
		return false;

	// All corners inside the volume.
	if (OutcodesOR == 0)
		return true;

	int Indices[36] =
	{
		0, 1, 2,
		0, 2, 3,
		7, 6, 5,
		7, 5, 4,
		0, 1, 5,
		0, 5, 4,
		2, 3, 7,
		2, 7, 6,
		0, 3, 7,
		0, 7, 4,
		1, 2, 6,
		1, 6, 5
	};

	for (int Idx = 0; Idx < 36; Idx += 3)
		if (swTriangleIntersectsPlanes(Points[Indices[Idx]*3], Points[Indices[Idx+1]*3], Points[Indices[Idx+2]*3], Planes))
			return true;

	return false;
}

struct swBoundingBox
{
	swVector3 Min;
	swVector3 Max;
};

inline void swGetBoxCorners(const swVector3& Min, const swVector3& Max, swVector3 (&Points)[8])
{
	Points[0] = swVector3(Max.x, Max.y, Min.z);
	Points[1] = swVector3(Min.x, Max.y, Min.z);
	Points[2] = swVector3(Max.x, Max.y, Max.z);
	Points[3] = swVector3(Min.x, Min.y, Min.z);
	Points[4] = swVector3(Min.x, Min.y, Max.z);
	Points[5] = swVector3(Max.x, Min.y, Max.z);
	Points[6] = swVector3(Max.x, Min.y, Min.z);
	Points[7] = swVector3(Min.x, Max.y, Max.z);
}

inline void swGetBoxCorners(const swBoundingBox& BoundingBox, swVector3 (&Points)[8])
{
	swGetBoxCorners(BoundingBox.Min, BoundingBox.Max, Points);
}

/*
bool SphereIntersectsVolume(const Vector3& Center, float Radius, const Vector4* Planes, int NumPlanes)
{
	for (int j = 0; j < NumPlanes; j++)
		if (Dot3(Center, Planes[j]) + Planes[j][3] > Radius)
			return false;

	return true;
}*/

inline swVector3 swRGBToHSL(const swVector3& rgb)
{
	int Mi;
	float M, m, C, h, S, L; // h is H/60

	Mi = (rgb[0] >= rgb[1]) ? 0 : 1;
	Mi = (rgb[Mi] >= rgb[2]) ? Mi : 2;
	M = rgb[Mi];

	m = (rgb[0] < rgb[1]) ? rgb[0] : rgb[1];
	m = (m < rgb[2]) ? m : rgb[2];

	C = M - m;
	L = (M + m) / 2.0f;

	if (C < SW_RGB_EPSILON) // C == 0.0
		h = 0.0f;
	else if (Mi == 0)       // M == R
		h = 0.0f + (rgb[1] - rgb[2]) / C;
	else if (Mi == 1)       // M == G
		h = 2.0f + (rgb[2] - rgb[0]) / C;
	else                    // M = B
		h = 4.0f + (rgb[0] - rgb[1]) / C;

	h = (h <  0.0) ? h + 6.0f : h;
	h = (h >= 6.0) ? h - 6.0f : h;

	S = ((L < (SW_RGB_EPSILON / 2.0f)) || (L > (1.0f -(SW_RGB_EPSILON / 2.0f))))
		? 0.0f : (2.0f * (M - L)) / (1.0f - fabs((2.0f * L) - 1.0f)) ;

	return swVector3(h, S, L);
}

inline swVector3 swHSLToRGB(const swVector3& hSL)
{
	swVector3 rgb;
	float h, S, L, C, X, m;

	h = hSL[0];
	S = hSL[1];
	L = hSL[2];

	C = (1.0f - fabs(2.0f * L - 1.0f)) * S;
	X = C * (1.0f - fabs(fmodf(h, 2.0f) - 1.0f));

	if (h < 1.0f)
		rgb = swVector3(C, X, 0.0f);
	else if (h < 2.0f)
		rgb = swVector3(X, C, 0.0f);
	else if (h < 3.0f)
		rgb = swVector3(0.0f, C, X);
	else if (h < 4.0f)
		rgb = swVector3(0.0f, X, C);
	else if (h < 5.0f)
		rgb = swVector3(X, 0.0f, C);
	else
		rgb = swVector3(C, 0.0f, X);

	m = L - C / 2.0f;
	rgb += m;

	return rgb;
}

inline swVector4 swAlgorithmicEdgeColor(const swVector3& Value, const float ValueLum, const float EdgeLum, const float Contrast, const float Saturation)
{
	float y1, yt;
	swVector3 hSL, rgb1, rgbf;

	// Determine luma target
	if (EdgeLum < ValueLum)
	{
		// Light base color
		yt = ValueLum - Contrast * ValueLum;
	}
	else
	{
		// Dark base color
		yt = ValueLum + Contrast * (1.0f - ValueLum);
	}

	// Get base color in hSL
	hSL = swRGBToHSL(Value);

	// Adjust saturation
//	sat = 4.0f * sat - 2.0f;
//	if (sat < 0.0f)
//	{
//		sat = -sat;
//		hSL[0] = (hSL[0] < 3.0f) ? hSL[0] + 3.0f : hSL[0] - 3.0f;
//	}
//	sat = (sat > 2.0f) ? 2.0f : sat;
//	if (sat > 1.0f)
//	{
//		// Supersaturate
//		sat -= 1.0f;
//		hSL[1] += sat * (1.0f - hSL[1]);
//	}
//	else
//	{
		// Desaturate
		hSL[1] *= Saturation;
//	}

	// Adjusted color to RGB
	rgb1 = swHSLToRGB(swVector3(hSL[0], hSL[1], 0.5f));

	// Fix adjusted color luma to target value
	y1 = swLuminescence(rgb1);

	if (yt < y1)
	{
		// Make darker via scaling
		rgbf = (yt/y1) * rgb1;
	}
	else
	{
		// Make lighter via scaling anti-color
		rgbf = swVector3(1.0f, 1.0f, 1.0f) - rgb1;
		rgbf *= (1.0f - yt) / (1.0f - y1);
		rgbf = swVector3(1.0f, 1.0f, 1.0f) - rgbf;
	}

	return  swVector4(swLinearToSRGB(rgbf), 1.0f);
}

Q_DECLARE_METATYPE(swVector3);
