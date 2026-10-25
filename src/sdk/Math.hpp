#pragma once
#include <cmath>
#include <string>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244) // double <-> float in vector math
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static std::string ws2s(const std::wstring& w)
{
	std::string s;
	s.reserve(w.size());
	for (wchar_t c : w)
		s.push_back(c < 128 ? char(c) : '?');
	return s;
}


class Vector2
{
public:
	Vector2() : x(0.f), y(0.f) {}
	Vector2(float _x, float _y) : x(_x), y(_y) {}
	Vector2(double _x, double _y) : x(static_cast<float>(_x)), y(static_cast<float>(_y)) {}
	~Vector2() {}
	float x;
	float y;

	Vector2 operator+(const Vector2& o) const { return Vector2(x + o.x, y + o.y); }
	Vector2 operator-(const Vector2& o) const { return Vector2(x - o.x, y - o.y); }
	Vector2 operator*(float s) const { return Vector2(x * s, y * s); }
	Vector2 operator/(float s) const { return Vector2(x / s, y / s); }
	Vector2 operator+(float s) const { return Vector2(x + s, y + s); }
	Vector2 operator-(float s) const { return Vector2(x - s, y - s); }
	friend Vector2 operator*(float s, const Vector2& v) { return Vector2(v.x * s, v.y * s); }
	Vector2& operator+=(const Vector2& o) { x += o.x; y += o.y; return *this; }
	Vector2& operator-=(const Vector2& o) { x -= o.x; y -= o.y; return *this; }

	Vector2 Floored() const { return Vector2(std::floor(x), std::floor(y)); }
	Vector2 Ceiled() const { return Vector2(std::ceil(x), std::ceil(y)); }
	Vector2 Rounded() const { return Vector2(std::round(x), std::round(y)); }
};


class Vector3
{
public:
	Vector3() : x(0.f), y(0.f), z(0.f) {}
	Vector3(double _x, double _y, double _z) : x(_x), y(_y), z(_z) {}
	~Vector3() {}

	double x;
	double y;
	double z;

	inline double dot(Vector3 v) const
	{
		return x * v.x + y * v.y + z * v.z;
	}

	inline double distance(Vector3 v) const
	{
		return double(sqrt(pow(v.x - x, 2.0) + pow(v.y - y, 2.0) + pow(v.z - z, 2.0)));
	}

	Vector3 operator-(Vector3 v) const
	{
		return Vector3(x - v.x, y - v.y, z - v.z);
	}

	Vector3 operator+(Vector3 v) const
	{
		return Vector3(x + v.x, y + v.y, z + v.z);
	}

	Vector3 operator*(double scalar) const
	{
		return Vector3(x * scalar, y * scalar, z * scalar);
	}

	inline Vector3 operator/(double scalar) const
	{

		return Vector3(x / scalar, y / scalar, z / scalar);
	}

	inline double Length() const
	{
		return sqrt(x * x + y * y + z * z);
	}

	bool IsZero(float epsilon = 0.01f) const
	{
		return fabs(x) < epsilon && fabs(y) < epsilon && fabs(z) < epsilon;
	}


	inline double Dot(const Vector3& v) const
	{
		return (x * v.x) + (y * v.y) + (z * v.z);
	}

	float Distance(const Vector3& a, const Vector3& b)
	{
		float dx = a.x - b.x;
		float dy = a.y - b.y;
		float dz = a.z - b.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

};

struct FQuat
{
	double x;
	double y;
	double z;
	double w;
};

struct FRotator {
	double Pitch;
	double Yaw;
	double Roll;
};

struct FLinearColor
{
	float R;
	float G;
	float B;
	float A;

	FLinearColor() : R(1.0f), G(1.0f), B(1.0f), A(1.0f) {}
	FLinearColor(float r, float g, float b, float a = 1.0f) : R(r), G(g), B(b), A(a) {}
};

struct FTransform
{
	FQuat rot;
	Vector3 translation;
	char pad[8];
	Vector3 scale;
	char pad1[8];
	D3DMATRIX ToMatrixWithScale()
	{

		if (scale.x == 0.f && scale.y == 0.f && scale.z == 0.f)
		{
			scale = { 1.f, 1.f, 1.f };
		}


		D3DMATRIX m{};
		m._41 = translation.x;
		m._42 = translation.y;
		m._43 = translation.z;
		float x2 = rot.x + rot.x;
		float y2 = rot.y + rot.y;
		float z2 = rot.z + rot.z;
		float xx2 = rot.x * x2;
		float yy2 = rot.y * y2;
		float zz2 = rot.z * z2;
		m._11 = (1.0f - (yy2 + zz2)) * scale.x;
		m._22 = (1.0f - (xx2 + zz2)) * scale.y;
		m._33 = (1.0f - (xx2 + yy2)) * scale.z;
		float yz2 = rot.y * z2;
		float wx2 = rot.w * x2;
		m._32 = (yz2 - wx2) * scale.z;
		m._23 = (yz2 + wx2) * scale.y;
		float xy2 = rot.x * y2;
		float wz2 = rot.w * z2;
		m._21 = (xy2 - wz2) * scale.y;
		m._12 = (xy2 + wz2) * scale.x;
		float xz2 = rot.x * z2;
		float wy2 = rot.w * y2;
		m._31 = (xz2 + wy2) * scale.z;
		m._13 = (xz2 - wy2) * scale.x;
		m._14 = 0.0f;
		m._24 = 0.0f;
		m._34 = 0.0f;
		m._44 = 1.0f;
		return m;
	}
};

D3DMATRIX MatrixMultiplication(D3DMATRIX pm1, D3DMATRIX pm2)
{
	D3DMATRIX pout{};
	pout._11 = pm1._11 * pm2._11 + pm1._12 * pm2._21 + pm1._13 * pm2._31 + pm1._14 * pm2._41;
	pout._12 = pm1._11 * pm2._12 + pm1._12 * pm2._22 + pm1._13 * pm2._32 + pm1._14 * pm2._42;
	pout._13 = pm1._11 * pm2._13 + pm1._12 * pm2._23 + pm1._13 * pm2._33 + pm1._14 * pm2._43;
	pout._14 = pm1._11 * pm2._14 + pm1._12 * pm2._24 + pm1._13 * pm2._34 + pm1._14 * pm2._44;
	pout._21 = pm1._21 * pm2._11 + pm1._22 * pm2._21 + pm1._23 * pm2._31 + pm1._24 * pm2._41;
	pout._22 = pm1._21 * pm2._12 + pm1._22 * pm2._22 + pm1._23 * pm2._32 + pm1._24 * pm2._42;
	pout._23 = pm1._21 * pm2._13 + pm1._22 * pm2._23 + pm1._23 * pm2._33 + pm1._24 * pm2._43;
	pout._24 = pm1._21 * pm2._14 + pm1._22 * pm2._24 + pm1._23 * pm2._34 + pm1._24 * pm2._44;
	pout._31 = pm1._31 * pm2._11 + pm1._32 * pm2._21 + pm1._33 * pm2._31 + pm1._34 * pm2._41;
	pout._32 = pm1._31 * pm2._12 + pm1._32 * pm2._22 + pm1._33 * pm2._32 + pm1._34 * pm2._42;
	pout._33 = pm1._31 * pm2._13 + pm1._32 * pm2._23 + pm1._33 * pm2._33 + pm1._34 * pm2._43;
	pout._34 = pm1._31 * pm2._14 + pm1._32 * pm2._24 + pm1._33 * pm2._34 + pm1._34 * pm2._44;
	pout._41 = pm1._41 * pm2._11 + pm1._42 * pm2._21 + pm1._43 * pm2._31 + pm1._44 * pm2._41;
	pout._42 = pm1._41 * pm2._12 + pm1._42 * pm2._22 + pm1._43 * pm2._32 + pm1._44 * pm2._42;
	pout._43 = pm1._41 * pm2._13 + pm1._42 * pm2._23 + pm1._43 * pm2._33 + pm1._44 * pm2._43;
	pout._44 = pm1._41 * pm2._14 + pm1._42 * pm2._24 + pm1._43 * pm2._34 + pm1._44 * pm2._44;
	return pout;
}

D3DMATRIX ToMatrix(Vector3 rot, Vector3 origin = Vector3(0, 0, 0))
{
	float RadPitch = (rot.x * M_PI / 180);
	float RadYaw = (rot.y * M_PI / 180);
	float RadRoll = (rot.z * M_PI / 180);
	float SP = sinf(RadPitch);
	float CP = cosf(RadPitch);
	float SY = sinf(RadYaw);
	float CY = cosf(RadYaw);
	float SR = sinf(RadRoll);
	float CR = cosf(RadRoll);
	D3DMATRIX Matrix{};
	Matrix.m[0][0] = CP * CY;
	Matrix.m[0][1] = CP * SY;
	Matrix.m[0][2] = SP;
	Matrix.m[0][3] = 0.f;
	Matrix.m[1][0] = SR * SP * CY - CR * SY;
	Matrix.m[1][1] = SR * SP * SY + CR * CY;
	Matrix.m[1][2] = -SR * CP;
	Matrix.m[1][3] = 0.f;
	Matrix.m[2][0] = -(CR * SP * CY + SR * SY);
	Matrix.m[2][1] = CY * SR - CR * SP * SY;
	Matrix.m[2][2] = CR * CP;
	Matrix.m[2][3] = 0.f;
	Matrix.m[3][0] = origin.x;
	Matrix.m[3][1] = origin.y;
	Matrix.m[3][2] = origin.z;
	Matrix.m[3][3] = 1.f;
	return Matrix;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
