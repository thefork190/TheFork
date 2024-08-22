/*
   Copyright (C) 2006, 2007 Sony Computer Entertainment Inc.
   All rights reserved.

   Redistribution and use in source and binary forms,
   with or without modification, are permitted provided that the
   following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Sony Computer Entertainment Inc nor the names
      of its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef VECTORMATH_SSE_MATRIX_HPP
#define VECTORMATH_SSE_MATRIX_HPP

namespace Vectormath
{
namespace SSE
{

// ========================================================
// Matrix3
// ========================================================

inline Matrix3::Matrix3(const Matrix3 & mat)
{
    mCol0 = mat.mCol0;
    mCol1 = mat.mCol1;
    mCol2 = mat.mCol2;
}

inline Matrix3::Matrix3(float scalar)
{
    mCol0 = Vector3(scalar);
    mCol1 = Vector3(scalar);
    mCol2 = Vector3(scalar);
}

inline Matrix3::Matrix3(const FloatInVec & scalar)
{
    mCol0 = Vector3(scalar);
    mCol1 = Vector3(scalar);
    mCol2 = Vector3(scalar);
}

inline Matrix3::Matrix3(const Quat & unitQuat)
{
    __m128 xyzw_2, wwww, yzxw, zxyw, yzxw_2, zxyw_2;
    __m128 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;

    VECTORMATH_ALIGNED(unsigned int sx[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int sz[4]) = { 0, 0, 0xFFFFFFFF, 0 };

    __m128 select_x = _mm_load_ps((float *)sx);
    __m128 select_z = _mm_load_ps((float *)sz);

    xyzw_2 = _mm_add_ps(unitQuat.get128(), unitQuat.get128());
    wwww = _mm_shuffle_ps(unitQuat.get128(), unitQuat.get128(), _MM_SHUFFLE(3, 3, 3, 3));
    yzxw = _mm_shuffle_ps(unitQuat.get128(), unitQuat.get128(), _MM_SHUFFLE(3, 0, 2, 1));
    zxyw = _mm_shuffle_ps(unitQuat.get128(), unitQuat.get128(), _MM_SHUFFLE(3, 1, 0, 2));
    yzxw_2 = _mm_shuffle_ps(xyzw_2, xyzw_2, _MM_SHUFFLE(3, 0, 2, 1));
    zxyw_2 = _mm_shuffle_ps(xyzw_2, xyzw_2, _MM_SHUFFLE(3, 1, 0, 2));

    tmp0 = _mm_mul_ps(yzxw_2, wwww);                                // tmp0 = 2yw, 2zw, 2xw, 2w2
    tmp1 = _mm_sub_ps(_mm_set1_ps(1.0f), _mm_mul_ps(yzxw, yzxw_2)); // tmp1 = 1 - 2y2, 1 - 2z2, 1 - 2x2, 1 - 2w2
    tmp2 = _mm_mul_ps(yzxw, xyzw_2);                                // tmp2 = 2xy, 2yz, 2xz, 2w2
    tmp0 = _mm_add_ps(_mm_mul_ps(zxyw, xyzw_2), tmp0);              // tmp0 = 2yw + 2zx, 2zw + 2xy, 2xw + 2yz, 2w2 + 2w2
    tmp1 = _mm_sub_ps(tmp1, _mm_mul_ps(zxyw, zxyw_2));              // tmp1 = 1 - 2y2 - 2z2, 1 - 2z2 - 2x2, 1 - 2x2 - 2y2, 1 - 2w2 - 2w2
    tmp2 = _mm_sub_ps(tmp2, _mm_mul_ps(zxyw_2, wwww));              // tmp2 = 2xy - 2zw, 2yz - 2xw, 2xz - 2yw, 2w2 -2w2

    tmp3 = sseSelect(tmp0, tmp1, select_x);
    tmp4 = sseSelect(tmp1, tmp2, select_x);
    tmp5 = sseSelect(tmp2, tmp0, select_x);
    mCol0 = Vector3(sseSelect(tmp3, tmp2, select_z));
    mCol1 = Vector3(sseSelect(tmp4, tmp0, select_z));
    mCol2 = Vector3(sseSelect(tmp5, tmp1, select_z));
}

inline Matrix3::Matrix3(const Vector3 & _col0, const Vector3 & _col1, const Vector3 & _col2)
{
    mCol0 = _col0;
    mCol1 = _col1;
    mCol2 = _col2;
}

inline Matrix3 & Matrix3::setCol0(const Vector3 & _col0)
{
    mCol0 = _col0;
    return *this;
}

inline Matrix3 & Matrix3::setCol1(const Vector3 & _col1)
{
    mCol1 = _col1;
    return *this;
}

inline Matrix3 & Matrix3::setCol2(const Vector3 & _col2)
{
    mCol2 = _col2;
    return *this;
}

inline Matrix3 & Matrix3::setCol(int col, const Vector3 & vec)
{
    *(&mCol0 + col) = vec;
    return *this;
}

inline Matrix3 & Matrix3::setRow(int row, const Vector3 & vec)
{
    mCol0.setElem(row, vec.getElem(0));
    mCol1.setElem(row, vec.getElem(1));
    mCol2.setElem(row, vec.getElem(2));
    return *this;
}

inline Matrix3 & Matrix3::setElem(int col, int row, float val)
{
    (*this)[col].setElem(row, val);
    return *this;
}

inline Matrix3 & Matrix3::setElem(int col, int row, const FloatInVec & val)
{
    Vector3 tmpV3_0;
    tmpV3_0 = this->getCol(col);
    tmpV3_0.setElem(row, val);
    this->setCol(col, tmpV3_0);
    return *this;
}

inline const FloatInVec Matrix3::getElem(int col, int row) const
{
    return this->getCol(col).getElem(row);
}

inline const Vector3 Matrix3::getCol0() const
{
    return mCol0;
}

inline const Vector3 Matrix3::getCol1() const
{
    return mCol1;
}

inline const Vector3 Matrix3::getCol2() const
{
    return mCol2;
}

inline const Vector3 Matrix3::getCol(int col) const
{
    return *(&mCol0 + col);
}

inline const Vector3 Matrix3::getRow(int row) const
{
    return Vector3(mCol0.getElem(row), mCol1.getElem(row), mCol2.getElem(row));
}

inline Vector3 & Matrix3::operator[](int col)
{
    return *(&mCol0 + col);
}

inline const Vector3 Matrix3::operator[](int col) const
{
    return *(&mCol0 + col);
}

inline Matrix3 & Matrix3::operator = (const Matrix3 & mat)
{
    mCol0 = mat.mCol0;
    mCol1 = mat.mCol1;
    mCol2 = mat.mCol2;
    return *this;
}

inline const Matrix3 transpose(const Matrix3 & mat)
{
    __m128 tmp0, tmp1, res0, res1, res2;
    tmp0 = sseMergeH(mat.getCol0().get128(), mat.getCol2().get128());
    tmp1 = sseMergeL(mat.getCol0().get128(), mat.getCol2().get128());
    res0 = sseMergeH(tmp0, mat.getCol1().get128());
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    res1 = _mm_shuffle_ps(tmp0, tmp0, _MM_SHUFFLE(0, 3, 2, 2));
    res1 = sseSelect(res1, mat.getCol1().get128(), select_y);
    res2 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(0, 1, 1, 0));
    res2 = sseSelect(res2, sseSplat(mat.getCol1().get128(), 2), select_y);
    return Matrix3(Vector3(res0), Vector3(res1), Vector3(res2));
}

inline const Matrix3 inverse(const Matrix3 & mat)
{
    __m128 tmp0, tmp1, tmp2, tmp3, tmp4, dot, invdet, inv0, inv1, inv2;
    tmp2 = sseVecCross(mat.getCol0().get128(), mat.getCol1().get128());
    tmp0 = sseVecCross(mat.getCol1().get128(), mat.getCol2().get128());
    tmp1 = sseVecCross(mat.getCol2().get128(), mat.getCol0().get128());
    dot = sseVecDot3(tmp2, mat.getCol2().get128());
    dot = sseSplat(dot, 0);
    invdet = sseRecipf(dot);
    tmp3 = sseMergeH(tmp0, tmp2);
    tmp4 = sseMergeL(tmp0, tmp2);
    inv0 = sseMergeH(tmp3, tmp1);
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    inv1 = _mm_shuffle_ps(tmp3, tmp3, _MM_SHUFFLE(0, 3, 2, 2));
    inv1 = sseSelect(inv1, tmp1, select_y);
    inv2 = _mm_shuffle_ps(tmp4, tmp4, _MM_SHUFFLE(0, 1, 1, 0));
    inv2 = sseSelect(inv2, sseSplat(tmp1, 2), select_y);
    inv0 = _mm_mul_ps(inv0, invdet);
    inv1 = _mm_mul_ps(inv1, invdet);
    inv2 = _mm_mul_ps(inv2, invdet);
    return Matrix3(Vector3(inv0), Vector3(inv1), Vector3(inv2));
}

inline const FloatInVec determinant(const Matrix3 & mat)
{
    return dot(mat.getCol2(), cross(mat.getCol0(), mat.getCol1()));
}

inline const Matrix3 Matrix3::operator + (const Matrix3 & mat) const
{
    return Matrix3((mCol0 + mat.mCol0),
                   (mCol1 + mat.mCol1),
                   (mCol2 + mat.mCol2));
}

inline const Matrix3 Matrix3::operator - (const Matrix3 & mat) const
{
    return Matrix3((mCol0 - mat.mCol0),
                   (mCol1 - mat.mCol1),
                   (mCol2 - mat.mCol2));
}

inline Matrix3 & Matrix3::operator += (const Matrix3 & mat)
{
    *this = *this + mat;
    return *this;
}

inline Matrix3 & Matrix3::operator -= (const Matrix3 & mat)
{
    *this = *this - mat;
    return *this;
}

inline const Matrix3 Matrix3::operator - () const
{
    return Matrix3((-mCol0), (-mCol1), (-mCol2));
}

inline const Matrix3 absPerElem(const Matrix3 & mat)
{
    return Matrix3(absPerElem(mat.getCol0()),
                   absPerElem(mat.getCol1()),
                   absPerElem(mat.getCol2()));
}

inline const Matrix3 Matrix3::operator * (float scalar) const
{
    return *this * FloatInVec(scalar);
}

inline const Matrix3 Matrix3::operator * (const FloatInVec & scalar) const
{
    return Matrix3((mCol0 * scalar),
                   (mCol1 * scalar),
                   (mCol2 * scalar));
}

inline Matrix3 & Matrix3::operator *= (float scalar)
{
    return *this *= FloatInVec(scalar);
}

inline Matrix3 & Matrix3::operator *= (const FloatInVec & scalar)
{
    *this = *this * scalar;
    return *this;
}

inline const Matrix3 operator * (float scalar, const Matrix3 & mat)
{
    return FloatInVec(scalar) * mat;
}

inline const Matrix3 operator * (const FloatInVec & scalar, const Matrix3 & mat)
{
    return mat * scalar;
}

inline const Vector3 Matrix3::operator * (const Vector3 & vec) const
{
    __m128 res;
    __m128 xxxx, yyyy, zzzz;
    xxxx = sseSplat(vec.get128(), 0);
    yyyy = sseSplat(vec.get128(), 1);
    zzzz = sseSplat(vec.get128(), 2);
    res = _mm_mul_ps(mCol0.get128(), xxxx);
    res = sseMAdd(mCol1.get128(), yyyy, res);
    res = sseMAdd(mCol2.get128(), zzzz, res);
    return Vector3(res);
}

inline const Matrix3 Matrix3::operator * (const Matrix3 & mat) const
{
    return Matrix3((*this * mat.mCol0),
                   (*this * mat.mCol1),
                   (*this * mat.mCol2));
}

inline Matrix3 & Matrix3::operator *= (const Matrix3 & mat)
{
    *this = *this * mat;
    return *this;
}

inline const Matrix3 mulPerElem(const Matrix3 & mat0, const Matrix3 & mat1)
{
    return Matrix3(mulPerElem(mat0.getCol0(), mat1.getCol0()),
                   mulPerElem(mat0.getCol1(), mat1.getCol1()),
                   mulPerElem(mat0.getCol2(), mat1.getCol2()));
}

inline const Matrix3 Matrix3::identity()
{
    return Matrix3(Vector3::xAxis(),
                   Vector3::yAxis(),
                   Vector3::zAxis());
}

inline const Matrix3 Matrix3::rotationX(float radians)
{
    return rotationX(FloatInVec(radians));
}

inline const Matrix3 Matrix3::rotationX(const FloatInVec & radians)
{
    __m128 s, c, res1, res2;
    __m128 zero;
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    zero = _mm_setzero_ps();
    sseSinfCosf(radians.get128(), &s, &c);
    res1 = sseSelect(zero, c, select_y);
    res1 = sseSelect(res1, s, select_z);
    res2 = sseSelect(zero, sseNegatef(s), select_y);
    res2 = sseSelect(res2, c, select_z);
    return Matrix3(Vector3::xAxis(), Vector3(res1), Vector3(res2));
}

inline const Matrix3 Matrix3::rotationY(float radians)
{
    return rotationY(FloatInVec(radians));
}

inline const Matrix3 Matrix3::rotationY(const FloatInVec & radians)
{
    __m128 s, c, res0, res2;
    __m128 zero;
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    zero = _mm_setzero_ps();
    sseSinfCosf(radians.get128(), &s, &c);
    res0 = sseSelect(zero, c, select_x);
    res0 = sseSelect(res0, sseNegatef(s), select_z);
    res2 = sseSelect(zero, s, select_x);
    res2 = sseSelect(res2, c, select_z);
    return Matrix3(Vector3(res0), Vector3::yAxis(), Vector3(res2));
}

inline const Matrix3 Matrix3::rotationZ(float radians)
{
    return rotationZ(FloatInVec(radians));
}

inline const Matrix3 Matrix3::rotationZ(const FloatInVec & radians)
{
    __m128 s, c, res0, res1;
    __m128 zero;
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    zero = _mm_setzero_ps();
    sseSinfCosf(radians.get128(), &s, &c);
    res0 = sseSelect(zero, c, select_x);
    res0 = sseSelect(res0, s, select_y);
    res1 = sseSelect(zero, sseNegatef(s), select_x);
    res1 = sseSelect(res1, c, select_y);
    return Matrix3(Vector3(res0), Vector3(res1), Vector3::zAxis());
}

inline const Matrix3 Matrix3::rotationZYX(const Vector3 & radiansXYZ)
{
    __m128 angles, s, negS, c, X0, X1, Y0, Y1, Z0, Z1, tmp;
    angles = Vector4(radiansXYZ, 0.0f).get128();
    sseSinfCosf(angles, &s, &c);
    negS = sseNegatef(s);
    Z0 = sseMergeL(c, s);
    Z1 = sseMergeL(negS, c);
    VECTORMATH_ALIGNED(unsigned int select_xyz[4]) = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0 };
    Z1 = _mm_and_ps(Z1, _mm_load_ps((float *)select_xyz));
    Y0 = _mm_shuffle_ps(c, negS, _MM_SHUFFLE(0, 1, 1, 1));
    Y1 = _mm_shuffle_ps(s, c, _MM_SHUFFLE(0, 1, 1, 1));
    X0 = sseSplat(s, 0);
    X1 = sseSplat(c, 0);
    tmp = _mm_mul_ps(Z0, Y1);
    return Matrix3(Vector3(_mm_mul_ps(Z0, Y0)),
                   Vector3(sseMAdd(Z1, X1, _mm_mul_ps(tmp, X0))),
                   Vector3(sseMSub(Z1, X0, _mm_mul_ps(tmp, X1))));
}

inline const Matrix3 Matrix3::rotation(float radians, const Vector3 & unitVec)
{
    return rotation(FloatInVec(radians), unitVec);
}

inline const Matrix3 Matrix3::rotation(const FloatInVec & radians, const Vector3 & unitVec)
{
    __m128 axis, s, c, oneMinusC, axisS, negAxisS, xxxx, yyyy, zzzz, tmp0, tmp1, tmp2;
    axis = unitVec.get128();
    sseSinfCosf(radians.get128(), &s, &c);
    xxxx = sseSplat(axis, 0);
    yyyy = sseSplat(axis, 1);
    zzzz = sseSplat(axis, 2);
    oneMinusC = _mm_sub_ps(_mm_set1_ps(1.0f), c);
    axisS = _mm_mul_ps(axis, s);
    negAxisS = sseNegatef(axisS);
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    tmp0 = _mm_shuffle_ps(axisS, axisS, _MM_SHUFFLE(0, 0, 2, 0));
    tmp0 = sseSelect(tmp0, sseSplat(negAxisS, 1), select_z);
    tmp1 = sseSelect(sseSplat(axisS, 0), sseSplat(negAxisS, 2), select_x);
    tmp2 = _mm_shuffle_ps(axisS, axisS, _MM_SHUFFLE(0, 0, 0, 1));
    tmp2 = sseSelect(tmp2, sseSplat(negAxisS, 0), select_y);
    tmp0 = sseSelect(tmp0, c, select_x);
    tmp1 = sseSelect(tmp1, c, select_y);
    tmp2 = sseSelect(tmp2, c, select_z);
    return Matrix3(Vector3(sseMAdd(_mm_mul_ps(axis, xxxx), oneMinusC, tmp0)),
                   Vector3(sseMAdd(_mm_mul_ps(axis, yyyy), oneMinusC, tmp1)),
                   Vector3(sseMAdd(_mm_mul_ps(axis, zzzz), oneMinusC, tmp2)));
}

inline const Matrix3 Matrix3::rotation(const Quat & unitQuat)
{
    return Matrix3(unitQuat);
}

inline const Matrix3 Matrix3::scale(const Vector3 & scaleVec)
{
    const __m128 zero = _mm_setzero_ps();
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    return Matrix3(Vector3(sseSelect(zero, scaleVec.get128(), select_x)),
                   Vector3(sseSelect(zero, scaleVec.get128(), select_y)),
                   Vector3(sseSelect(zero, scaleVec.get128(), select_z)));
}

inline const Matrix3 appendScale(const Matrix3 & mat, const Vector3 & scaleVec)
{
    return Matrix3((mat.getCol0() * scaleVec.getX()),
                   (mat.getCol1() * scaleVec.getY()),
                   (mat.getCol2() * scaleVec.getZ()));
}

inline const Matrix3 prependScale(const Vector3 & scaleVec, const Matrix3 & mat)
{
    return Matrix3(mulPerElem(mat.getCol0(), scaleVec),
                   mulPerElem(mat.getCol1(), scaleVec),
                   mulPerElem(mat.getCol2(), scaleVec));
}

inline const Matrix3 select(const Matrix3 & mat0, const Matrix3 & mat1, bool select1)
{
    return Matrix3(select(mat0.getCol0(), mat1.getCol0(), select1),
                   select(mat0.getCol1(), mat1.getCol1(), select1),
                   select(mat0.getCol2(), mat1.getCol2(), select1));
}

inline const Matrix3 select(const Matrix3 & mat0, const Matrix3 & mat1, const BoolInVec & select1)
{
    return Matrix3(select(mat0.getCol0(), mat1.getCol0(), select1),
                   select(mat0.getCol1(), mat1.getCol1(), select1),
                   select(mat0.getCol2(), mat1.getCol2(), select1));
}

#ifdef VECTORMATH_DEBUG

inline void print(const Matrix3 & mat)
{
    print(mat.getRow(0));
    print(mat.getRow(1));
    print(mat.getRow(2));
}

inline void print(const Matrix3 & mat, const char * name)
{
    std::printf("%s:\n", name);
    print(mat);
}

#endif // VECTORMATH_DEBUG

// ========================================================
// Matrix3d
// ========================================================

inline Matrix3d::Matrix3d(const Matrix3d & mat)
{
	mCol0 = mat.mCol0;
	mCol1 = mat.mCol1;
	mCol2 = mat.mCol2;
}

inline Matrix3d::Matrix3d(double scalar)
{
	mCol0 = Vector3d(scalar);
	mCol1 = Vector3d(scalar);
	mCol2 = Vector3d(scalar);
}

inline Matrix3d::Matrix3d(const DoubleInVec & scalar)
{
	mCol0 = Vector3d(scalar);
	mCol1 = Vector3d(scalar);
	mCol2 = Vector3d(scalar);
}

inline Matrix3d::Matrix3d(const Quat & unitQuat)
{
	const DSSEVec4 unitQuatd = dsseSetr((double)unitQuat.getX(), (double)unitQuat.getY(), (double)unitQuat.getZ(), (double)unitQuat.getW());
	DSSEVec4 xyzw_2, wwww, yzxw, zxyw, yzxw_2, zxyw_2;
	DSSEVec4 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;

	VECTORMATH_ALIGNED(unsigned long long sx[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long sz[4]) = { 0, 0, 0xFFFFFFFFFFFFFFFF, 0 };

	DSSEVec4 select_x = dsseLoadu((double *)sx);
	DSSEVec4 select_z = dsseLoadu((double *)sz);

	xyzw_2 = dsseAdd(unitQuatd, unitQuatd);
	wwww = dsseShuffle(unitQuatd, unitQuatd, _MM_SHUFFLE(3, 3, 3, 3));
	yzxw = dsseShuffle(unitQuatd, unitQuatd, _MM_SHUFFLE(3, 0, 2, 1));
	zxyw = dsseShuffle(unitQuatd, unitQuatd, _MM_SHUFFLE(3, 1, 0, 2));
	yzxw_2 = dsseShuffle(xyzw_2, xyzw_2, _MM_SHUFFLE(3, 0, 2, 1));
	zxyw_2 = dsseShuffle(xyzw_2, xyzw_2, _MM_SHUFFLE(3, 1, 0, 2));

	tmp0 = dsseMul(yzxw_2, wwww);                                // tmp0 = 2yw, 2zw, 2xw, 2w2
	tmp1 = dsseSub(dsseSet1(1.0), dsseMul(yzxw, yzxw_2)); // tmp1 = 1 - 2y2, 1 - 2z2, 1 - 2x2, 1 - 2w2
	tmp2 = dsseMul(yzxw, xyzw_2);                                // tmp2 = 2xy, 2yz, 2xz, 2w2
	tmp0 = dsseAdd(dsseMul(zxyw, xyzw_2), tmp0);              // tmp0 = 2yw + 2zx, 2zw + 2xy, 2xw + 2yz, 2w2 + 2w2
	tmp1 = dsseSub(tmp1, dsseMul(zxyw, zxyw_2));              // tmp1 = 1 - 2y2 - 2z2, 1 - 2z2 - 2x2, 1 - 2x2 - 2y2, 1 - 2w2 - 2w2
	tmp2 = dsseSub(tmp2, dsseMul(zxyw_2, wwww));              // tmp2 = 2xy - 2zw, 2yz - 2xw, 2xz - 2yw, 2w2 -2w2

	tmp3 = dsseSelect(tmp0, tmp1, select_x);
	tmp4 = dsseSelect(tmp1, tmp2, select_x);
	tmp5 = dsseSelect(tmp2, tmp0, select_x);
	mCol0 = Vector3d(dsseSelect(tmp3, tmp2, select_z));
	mCol1 = Vector3d(dsseSelect(tmp4, tmp0, select_z));
	mCol2 = Vector3d(dsseSelect(tmp5, tmp1, select_z));
}

inline Matrix3d::Matrix3d(const Vector3d & _col0, const Vector3d & _col1, const Vector3d & _col2)
{
	mCol0 = _col0;
	mCol1 = _col1;
	mCol2 = _col2;
}

inline Matrix3d & Matrix3d::setCol0(const Vector3d & _col0)
{
	mCol0 = _col0;
	return *this;
}

inline Matrix3d & Matrix3d::setCol1(const Vector3d & _col1)
{
	mCol1 = _col1;
	return *this;
}

inline Matrix3d & Matrix3d::setCol2(const Vector3d & _col2)
{
	mCol2 = _col2;
	return *this;
}

inline Matrix3d & Matrix3d::setCol(int col, const Vector3d & vec)
{
	*(&mCol0 + col) = vec;
	return *this;
}

inline Matrix3d & Matrix3d::setRow(int row, const Vector3d & vec)
{
	mCol0.setElem(row, vec.getElem(0));
	mCol1.setElem(row, vec.getElem(1));
	mCol2.setElem(row, vec.getElem(2));
	return *this;
}

inline Matrix3d & Matrix3d::setElem(int col, int row, double val)
{
	(*this)[col].setElem(row, val);
	return *this;
}

inline Matrix3d & Matrix3d::setElem(int col, int row, const DoubleInVec & val)
{
	Vector3d tmpV3_0;
	tmpV3_0 = this->getCol(col);
	tmpV3_0.setElem(row, val);
	this->setCol(col, tmpV3_0);
	return *this;
}

inline const DoubleInVec Matrix3d::getElem(int col, int row) const
{
	return this->getCol(col).getElem(row);
}

inline const Vector3d Matrix3d::getCol0() const
{
	return mCol0;
}

inline const Vector3d Matrix3d::getCol1() const
{
	return mCol1;
}

inline const Vector3d Matrix3d::getCol2() const
{
	return mCol2;
}

inline const Vector3d Matrix3d::getCol(int col) const
{
	return *(&mCol0 + col);
}

inline const Vector3d Matrix3d::getRow(int row) const
{
	return Vector3d(mCol0.getElem(row), mCol1.getElem(row), mCol2.getElem(row));
}

inline Vector3d & Matrix3d::operator[](int col)
{
	return *(&mCol0 + col);
}

inline const Vector3d Matrix3d::operator[](int col) const
{
	return *(&mCol0 + col);
}

inline Matrix3d & Matrix3d::operator = (const Matrix3d & mat)
{
	mCol0 = mat.mCol0;
	mCol1 = mat.mCol1;
	mCol2 = mat.mCol2;
	return *this;
}

inline const Matrix3d transpose(const Matrix3d & mat)
{
	
	DSSEVec4 tmp0, tmp1, res0, res1, res2;
	tmp0 = dsseMergeH(mat.getCol0().get256(), mat.getCol2().get256());
	tmp1 = dsseMergeL(mat.getCol0().get256(), mat.getCol2().get256());
	res0 = dsseMergeH(tmp0, mat.getCol1().get256());
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	res1 = dsseShuffle(tmp0, tmp0, _MM_SHUFFLE(0, 3, 2, 2));
	res1 = dsseSelect(res1, mat.getCol1().get256(), select_y);
	res2 = dsseShuffle(tmp1, tmp1, _MM_SHUFFLE(0, 1, 1, 0));
	res2 = dsseSelect(res2, dsseSplat(mat.getCol1().get256(), 2), select_y);
	return Matrix3d(Vector3d(res0), Vector3d(res1), Vector3d(res2));
}

inline const Matrix3d inverse(const Matrix3d & mat)
{
	DSSEVec4 tmp0, tmp1, tmp2, tmp3, tmp4, dot, invdet, inv0, inv1, inv2;
	tmp2 = dsseVecCross(mat.getCol0().get256(), mat.getCol1().get256());
	tmp0 = dsseVecCross(mat.getCol1().get256(), mat.getCol2().get256());
	tmp1 = dsseVecCross(mat.getCol2().get256(), mat.getCol0().get256());
	dot = dsseVecDot3(tmp2, mat.getCol2().get256());
	dot = dsseSplat(dot, 0);
	invdet = dsseRecipf(dot);
	tmp3 = dsseMergeH(tmp0, tmp2);
	tmp4 = dsseMergeL(tmp0, tmp2);
	inv0 = dsseMergeH(tmp3, tmp1);
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	inv1 = dsseShuffle(tmp3, tmp3, _MM_SHUFFLE(0, 3, 2, 2));
	inv1 = dsseSelect(inv1, tmp1, select_y);
	inv2 = dsseShuffle(tmp4, tmp4, _MM_SHUFFLE(0, 1, 1, 0));
	inv2 = dsseSelect(inv2, dsseSplat(tmp1, 2), select_y);
	inv0 = dsseMul(inv0, invdet);
	inv1 = dsseMul(inv1, invdet);
	inv2 = dsseMul(inv2, invdet);
	return Matrix3d(Vector3d(inv0), Vector3d(inv1), Vector3d(inv2));
}

inline const DoubleInVec determinant(const Matrix3d & mat)
{
	return dot(mat.getCol2(), cross(mat.getCol0(), mat.getCol1()));
}

inline const Matrix3d Matrix3d::operator + (const Matrix3d & mat) const
{
	return Matrix3d((mCol0 + mat.mCol0), (mCol1 + mat.mCol1), (mCol2 + mat.mCol2));
}

inline const Matrix3d Matrix3d::operator - (const Matrix3d & mat) const
{
	return Matrix3d((mCol0 - mat.mCol0), (mCol1 - mat.mCol1), (mCol2 - mat.mCol2));
}

inline Matrix3d & Matrix3d::operator += (const Matrix3d & mat)
{
	*this = *this + mat;
	return *this;
}

inline Matrix3d & Matrix3d::operator -= (const Matrix3d & mat)
{
	*this = *this - mat;
	return *this;
}

inline const Matrix3d Matrix3d::operator - () const
{
	return Matrix3d((-mCol0), (-mCol1), (-mCol2));
}

inline const Matrix3d absPerElem(const Matrix3d & mat)
{
	return Matrix3d(absPerElem(mat.getCol0()), absPerElem(mat.getCol1()), absPerElem(mat.getCol2()));
}

inline const Matrix3d Matrix3d::operator * (double scalar) const
{
	return *this * DoubleInVec(scalar);
}

inline const Matrix3d Matrix3d::operator * (const DoubleInVec & scalar) const
{
	return Matrix3d((mCol0 * scalar), (mCol1 * scalar), (mCol2 * scalar));
}

inline Matrix3d & Matrix3d::operator *= (double scalar)
{
	return *this *= DoubleInVec(scalar);
}

inline Matrix3d & Matrix3d::operator *= (const DoubleInVec & scalar)
{
	*this = *this * scalar;
	return *this;
}

inline const Matrix3d operator * (double scalar, const Matrix3d & mat)
{
	return DoubleInVec(scalar) * mat;
}

inline const Matrix3d operator * (const DoubleInVec & scalar, const Matrix3d & mat)
{
	return mat * scalar;
}

inline const Vector3d Matrix3d::operator * (const Vector3d & vec) const
{
		DSSEVec4 res;
		DSSEVec4 xxxx, yyyy, zzzz;
		xxxx = dsseSplat(vec.get256(), 0);
		yyyy = dsseSplat(vec.get256(), 1);
		zzzz = dsseSplat(vec.get256(), 2);
		res = dsseMul(mCol0.get256(), xxxx);
		res = dsseMAdd(mCol1.get256(), yyyy, res);
		res = dsseMAdd(mCol2.get256(), zzzz, res);
		return Vector3d(res);
}

inline const Matrix3d Matrix3d::operator * (const Matrix3d & mat) const
{
	return Matrix3d((*this * mat.mCol0), (*this * mat.mCol1), (*this * mat.mCol2));
}

inline Matrix3d & Matrix3d::operator *= (const Matrix3d & mat)
{
	*this = *this * mat;
	return *this;
}

inline const Matrix3d mulPerElem(const Matrix3d & mat0, const Matrix3d & mat1)
{
	return Matrix3d(
		mulPerElem(mat0.getCol0(), mat1.getCol0()),
		mulPerElem(mat0.getCol1(), mat1.getCol1()),
		mulPerElem(mat0.getCol2(), mat1.getCol2())
	);
}

inline const Matrix3d Matrix3d::identity()
{
	return Matrix3d(Vector3d::xAxis(), Vector3d::yAxis(), Vector3d::zAxis());
}

inline const Matrix3d Matrix3d::rotationX(double radians)
{
	return rotationX(DoubleInVec(radians));
}

inline const Matrix3d Matrix3d::rotationX(const DoubleInVec & radians)
{
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_z[4]) = { 0, 0, 0xFFFFFFFFFFFFFFFF, 0 };
	const DSSEVec4 zero = dsseSetZero();
	DSSEVec4 s, c, res1, res2;
	dsseSinfCosf(radians.get256(), &s, &c);
	res1 = dsseSelect(zero, c, select_y);
	res1 = dsseSelect(res1, s, select_z);
	res2 = dsseSelect(zero, dsseNegatef(s), select_y);
	res2 = dsseSelect(res2, c, select_z);
	return Matrix3d(Vector3d::xAxis(), Vector3d(res1), Vector3d(res2));
}

inline const Matrix3d Matrix3d::rotationY(double radians)
{
	return rotationY(DoubleInVec(radians));
}

inline const Matrix3d Matrix3d::rotationY(const DoubleInVec & radians)
{
	VECTORMATH_ALIGNED(unsigned long long select_x[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_z[4]) = { 0, 0, 0xFFFFFFFFFFFFFFFF, 0 };
	const DSSEVec4 zero = dsseSetZero();
	DSSEVec4 s, c, res0, res2;
	dsseSinfCosf(radians.get256(), &s, &c);
	res0 = dsseSelect(zero, c, select_x);
	res0 = dsseSelect(res0, dsseNegatef(s), select_z);
	res2 = dsseSelect(zero, s, select_x);
	res2 = dsseSelect(res2, c, select_z);
	return Matrix3d(Vector3d(res0), Vector3d::yAxis(), Vector3d(res2));
}

inline const Matrix3d Matrix3d::rotationZ(double radians)
{
	return rotationZ(DoubleInVec(radians));
}

inline const Matrix3d Matrix3d::rotationZ(const DoubleInVec & radians)
{
	VECTORMATH_ALIGNED(unsigned long long select_x[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	const DSSEVec4 zero = dsseSetZero();
	DSSEVec4 s, c, res0, res1;
	dsseSinfCosf(radians.get256(), &s, &c);
	res0 = dsseSelect(zero, c, select_x);
	res0 = dsseSelect(res0, s, select_y);
	res1 = dsseSelect(zero, dsseNegatef(s), select_x);
	res1 = dsseSelect(res1, c, select_y);
	return Matrix3d(Vector3d(res0), Vector3d(res1), Vector3d::zAxis());
}

inline const Matrix3d Matrix3d::rotationZYX(const Vector3d & radiansXYZ)
{
	DSSEVec4 angles, s, negS, c, X0, X1, Y0, Y1, Z0, Z1, tmp;
	angles = Vector4d(radiansXYZ, 0.0).get256();
	dsseSinfCosf(angles, &s, &c);
	negS = dsseNegatef(s);
	Z0 = dsseMergeL(c, s);
	Z1 = dsseMergeL(negS, c);
	VECTORMATH_ALIGNED(unsigned long long select_xyz[4]) = { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0 };
	Z1 = dsseAnd(Z1, dsseLoadu((double *)select_xyz));
	Y0 = dsseShuffle(c, negS, _MM_SHUFFLE(0, 1, 1, 1));
	Y1 = dsseShuffle(s, c, _MM_SHUFFLE(0, 1, 1, 1));
	X0 = dsseSplat(s, 0);
	X1 = dsseSplat(c, 0);
	tmp = dsseMul(Z0, Y1);
	return Matrix3d(
		Vector3d(dsseMul(Z0, Y0)),
		Vector3d(dsseMAdd(Z1, X1, dsseMul(tmp, X0))),
		Vector3d(dsseMSub(Z1, X0, dsseMul(tmp, X1)))
	);
}

inline const Matrix3d Matrix3d::rotation(double radians, const Vector3d & unitVec)
{
	return rotation(DoubleInVec(radians), unitVec);
}

inline const Matrix3d Matrix3d::rotation(const DoubleInVec & radians, const Vector3d & unitVec)
{
	DSSEVec4 axis, s, c, oneMinusC, axisS, negAxisS, xxxx, yyyy, zzzz, tmp0, tmp1, tmp2;
	axis = unitVec.get256();
	dsseSinfCosf(radians.get256(), &s, &c);
	xxxx = dsseSplat(axis, 0);
	yyyy = dsseSplat(axis, 1);
	zzzz = dsseSplat(axis, 2);
	oneMinusC = dsseSub(dsseSet1(1.0), c);
	axisS = dsseMul(axis, s);
	negAxisS = dsseNegatef(axisS);
	VECTORMATH_ALIGNED(unsigned long long select_x[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_z[4]) = { 0, 0, 0xFFFFFFFFFFFFFFFF, 0 };
	tmp0 = dsseShuffle(axisS, axisS, _MM_SHUFFLE(0, 0, 2, 0));
	tmp0 = dsseSelect(tmp0, dsseSplat(negAxisS, 1), select_z);
	tmp1 = dsseSelect(dsseSplat(axisS, 0), dsseSplat(negAxisS, 2), select_x);
	tmp2 = dsseShuffle(axisS, axisS, _MM_SHUFFLE(0, 0, 0, 1));
	tmp2 = dsseSelect(tmp2, dsseSplat(negAxisS, 0), select_y);
	tmp0 = dsseSelect(tmp0, c, select_x);
	tmp1 = dsseSelect(tmp1, c, select_y);
	tmp2 = dsseSelect(tmp2, c, select_z);
	return Matrix3d(
		Vector3d(dsseMAdd(dsseMul(axis, xxxx), oneMinusC, tmp0)),
		Vector3d(dsseMAdd(dsseMul(axis, yyyy), oneMinusC, tmp1)),
		Vector3d(dsseMAdd(dsseMul(axis, zzzz), oneMinusC, tmp2))
	);
}

inline const Matrix3d Matrix3d::rotation(const Quat & unitQuat)
{
	return Matrix3d(unitQuat);
}

inline const Matrix3d Matrix3d::scale(const Vector3d & scaleVec)
{
	const DSSEVec4 zero = dsseSetZero();
	VECTORMATH_ALIGNED(unsigned long long select_x[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_z[4]) = { 0, 0, 0xFFFFFFFFFFFFFFFF, 0 };
	return Matrix3d(
		Vector3d(dsseSelect(zero, scaleVec.get256(), select_x)),
		Vector3d(dsseSelect(zero, scaleVec.get256(), select_y)),
		Vector3d(dsseSelect(zero, scaleVec.get256(), select_z))
	);
}

inline const Matrix3d appendScale(const Matrix3d & mat, const Vector3d & scaleVec)
{
	return Matrix3d(
		(mat.getCol0() * scaleVec.getX()),
		(mat.getCol1() * scaleVec.getY()),
		(mat.getCol2() * scaleVec.getZ())
	);
}

inline const Matrix3d prependScale(const Vector3d & scaleVec, const Matrix3d & mat)
{
	return Matrix3d(
		mulPerElem(mat.getCol0(), scaleVec),
		mulPerElem(mat.getCol1(), scaleVec),
		mulPerElem(mat.getCol2(), scaleVec)
	);
}

inline const Matrix3d select(const Matrix3d & mat0, const Matrix3d & mat1, bool select1)
{
	return Matrix3d(
		select(mat0.getCol0(), mat1.getCol0(), select1),
		select(mat0.getCol1(), mat1.getCol1(), select1),
		select(mat0.getCol2(), mat1.getCol2(), select1)
	);
}

inline const Matrix3d select(const Matrix3d & mat0, const Matrix3d & mat1, const BoolInVec & select1)
{
	return Matrix3d(
		select(mat0.getCol0(), mat1.getCol0(), select1),
		select(mat0.getCol1(), mat1.getCol1(), select1),
		select(mat0.getCol2(), mat1.getCol2(), select1)
	);
}

#ifdef VECTORMATH_DEBUG

inline void print(const Matrix3d & mat)
{
	print(mat.getRow(0));
	print(mat.getRow(1));
	print(mat.getRow(2));
}

inline void print(const Matrix3d & mat, const char * name)
{
	std::printf("%s:\n", name);
	print(mat);
}

#endif // VECTORMATH_DEBUG

// ========================================================
// Matrix4
// ========================================================

inline Matrix4::Matrix4(const Matrix4 & mat)
{
    mCol0 = mat.mCol0;
    mCol1 = mat.mCol1;
    mCol2 = mat.mCol2;
    mCol3 = mat.mCol3;
}

inline Matrix4::Matrix4(float scalar)
{
    mCol0 = Vector4(scalar);
    mCol1 = Vector4(scalar);
    mCol2 = Vector4(scalar);
    mCol3 = Vector4(scalar);
}

inline Matrix4::Matrix4(const FloatInVec & scalar)
{
    mCol0 = Vector4(scalar);
    mCol1 = Vector4(scalar);
    mCol2 = Vector4(scalar);
    mCol3 = Vector4(scalar);
}

inline Matrix4::Matrix4(const Transform3 & mat)
{
    mCol0 = Vector4(mat.getCol0(), 0.0f);
    mCol1 = Vector4(mat.getCol1(), 0.0f);
    mCol2 = Vector4(mat.getCol2(), 0.0f);
    mCol3 = Vector4(mat.getCol3(), 1.0f);
}

inline Matrix4::Matrix4(const Vector4 & _col0, const Vector4 & _col1, const Vector4 & _col2, const Vector4 & _col3)
{
    mCol0 = _col0;
    mCol1 = _col1;
    mCol2 = _col2;
    mCol3 = _col3;
}

inline Matrix4::Matrix4(const Matrix3 & mat, const Vector3 & translateVec)
{
    mCol0 = Vector4(mat.getCol0(), 0.0f);
    mCol1 = Vector4(mat.getCol1(), 0.0f);
    mCol2 = Vector4(mat.getCol2(), 0.0f);
    mCol3 = Vector4(translateVec,  1.0f);
}

inline Matrix4::Matrix4(const Quat & unitQuat, const Vector3 & translateVec)
{
    Matrix3 mat;
    mat = Matrix3(unitQuat);
    mCol0 = Vector4(mat.getCol0(), 0.0f);
    mCol1 = Vector4(mat.getCol1(), 0.0f);
    mCol2 = Vector4(mat.getCol2(), 0.0f);
    mCol3 = Vector4(translateVec,  1.0f);
}

inline Matrix4 & Matrix4::setCol0(const Vector4 & _col0)
{
    mCol0 = _col0;
    return *this;
}

inline Matrix4 & Matrix4::setCol1(const Vector4 & _col1)
{
    mCol1 = _col1;
    return *this;
}

inline Matrix4 & Matrix4::setCol2(const Vector4 & _col2)
{
    mCol2 = _col2;
    return *this;
}

inline Matrix4 & Matrix4::setCol3(const Vector4 & _col3)
{
    mCol3 = _col3;
    return *this;
}

inline Matrix4 & Matrix4::setCol(int col, const Vector4 & vec)
{
    *(&mCol0 + col) = vec;
    return *this;
}

inline Matrix4 & Matrix4::setRow(int row, const Vector4 & vec)
{
    mCol0.setElem(row, vec.getElem(0));
    mCol1.setElem(row, vec.getElem(1));
    mCol2.setElem(row, vec.getElem(2));
    mCol3.setElem(row, vec.getElem(3));
    return *this;
}

inline Matrix4 & Matrix4::setElem(int col, int row, float val)
{
    (*this)[col].setElem(row, val);
    return *this;
}

inline Matrix4 & Matrix4::setElem(int col, int row, const FloatInVec & val)
{
    Vector4 tmpV3_0;
    tmpV3_0 = this->getCol(col);
    tmpV3_0.setElem(row, val);
    this->setCol(col, tmpV3_0);
    return *this;
}

inline const FloatInVec Matrix4::getElem(int col, int row) const
{
    return this->getCol(col).getElem(row);
}

inline const Vector4 Matrix4::getCol0() const
{
    return mCol0;
}

inline const Vector4 Matrix4::getCol1() const
{
    return mCol1;
}

inline const Vector4 Matrix4::getCol2() const
{
    return mCol2;
}

inline const Vector4 Matrix4::getCol3() const
{
    return mCol3;
}

inline const Vector4 Matrix4::getCol(int col) const
{
    return *(&mCol0 + col);
}

inline const Vector4 Matrix4::getRow(int row) const
{
    return Vector4(mCol0.getElem(row), mCol1.getElem(row), mCol2.getElem(row), mCol3.getElem(row));
}

inline Vector4 & Matrix4::operator[](int col)
{
    return *(&mCol0 + col);
}

inline const Vector4 Matrix4::operator[](int col) const
{
    return *(&mCol0 + col);
}

inline Matrix4 & Matrix4::operator = (const Matrix4 & mat)
{
    mCol0 = mat.mCol0;
    mCol1 = mat.mCol1;
    mCol2 = mat.mCol2;
    mCol3 = mat.mCol3;
    return *this;
}

inline const Matrix4 transpose(const Matrix4 & mat)
{
    __m128 tmp0, tmp1, tmp2, tmp3, res0, res1, res2, res3;
    tmp0 = sseMergeH(mat.getCol0().get128(), mat.getCol2().get128());
    tmp1 = sseMergeH(mat.getCol1().get128(), mat.getCol3().get128());
    tmp2 = sseMergeL(mat.getCol0().get128(), mat.getCol2().get128());
    tmp3 = sseMergeL(mat.getCol1().get128(), mat.getCol3().get128());
    res0 = sseMergeH(tmp0, tmp1);
    res1 = sseMergeL(tmp0, tmp1);
    res2 = sseMergeH(tmp2, tmp3);
    res3 = sseMergeL(tmp2, tmp3);
    return Matrix4(Vector4(res0), Vector4(res1), Vector4(res2), Vector4(res3));
}

inline const Matrix4 inverse(const Matrix4 & mat)
{
    VECTORMATH_ALIGNED(unsigned int PNPN[4]) = { 0x00000000, 0x80000000, 0x00000000, 0x80000000 };
    VECTORMATH_ALIGNED(unsigned int NPNP[4]) = { 0x80000000, 0x00000000, 0x80000000, 0x00000000 };
    VECTORMATH_ALIGNED(float X1_YZ0_W1[4])   = { 1.0f, 0.0f, 0.0f, 1.0f };

    __m128 Va, Vb, Vc;
    __m128 r1, r2, r3, tt, tt2;
    __m128 sum, Det, RDet;
    __m128 trns0, trns1, trns2, trns3;

    __m128 _L1 = mat.getCol0().get128();
    __m128 _L2 = mat.getCol1().get128();
    __m128 _L3 = mat.getCol2().get128();
    __m128 _L4 = mat.getCol3().get128();
    // Calculating the minterms for the first line.

    // sseRor is just a macro using _mm_shuffle_ps().
    tt = _L4;
    tt2 = sseRor(_L3, 1);
    Vc = _mm_mul_ps(tt2, sseRor(tt, 0)); // V3'·V4
    Va = _mm_mul_ps(tt2, sseRor(tt, 2)); // V3'·V4"
    Vb = _mm_mul_ps(tt2, sseRor(tt, 3)); // V3'·V4^

    r1 = _mm_sub_ps(sseRor(Va, 1), sseRor(Vc, 2)); // V3"·V4^ - V3^·V4"
    r2 = _mm_sub_ps(sseRor(Vb, 2), sseRor(Vb, 0)); // V3^·V4' - V3'·V4^
    r3 = _mm_sub_ps(sseRor(Va, 0), sseRor(Vc, 1)); // V3'·V4" - V3"·V4'

    tt = _L2;
    Va = sseRor(tt, 1);
    sum = _mm_mul_ps(Va, r1);
    Vb = sseRor(tt, 2);
    sum = _mm_add_ps(sum, _mm_mul_ps(Vb, r2));
    Vc = sseRor(tt, 3);
    sum = _mm_add_ps(sum, _mm_mul_ps(Vc, r3));

    // Calculating the determinant.
    Det = _mm_mul_ps(sum, _L1);
    Det = _mm_add_ps(Det, _mm_movehl_ps(Det, Det));

    const __m128 Sign_PNPN = _mm_load_ps((float *)PNPN);
    const __m128 Sign_NPNP = _mm_load_ps((float *)NPNP);

    __m128 mtL1 = _mm_xor_ps(sum, Sign_PNPN);

    // Calculating the minterms of the second line (using previous results).
    tt = sseRor(_L1, 1);
    sum = _mm_mul_ps(tt, r1);
    tt = sseRor(tt, 1);
    sum = _mm_add_ps(sum, _mm_mul_ps(tt, r2));
    tt = sseRor(tt, 1);
    sum = _mm_add_ps(sum, _mm_mul_ps(tt, r3));
    __m128 mtL2 = _mm_xor_ps(sum, Sign_NPNP);

    // Testing the determinant.
    Det = _mm_sub_ss(Det, _mm_shuffle_ps(Det, Det, 1));

    // Calculating the minterms of the third line.
    tt = sseRor(_L1, 1);
    Va = _mm_mul_ps(tt, Vb);  // V1'·V2"
    Vb = _mm_mul_ps(tt, Vc);  // V1'·V2^
    Vc = _mm_mul_ps(tt, _L2); // V1'·V2

    r1 = _mm_sub_ps(sseRor(Va, 1), sseRor(Vc, 2)); // V1"·V2^ - V1^·V2"
    r2 = _mm_sub_ps(sseRor(Vb, 2), sseRor(Vb, 0)); // V1^·V2' - V1'·V2^
    r3 = _mm_sub_ps(sseRor(Va, 0), sseRor(Vc, 1)); // V1'·V2" - V1"·V2'

    tt = sseRor(_L4, 1);
    sum = _mm_mul_ps(tt, r1);
    tt = sseRor(tt, 1);
    sum = _mm_add_ps(sum, _mm_mul_ps(tt, r2));
    tt = sseRor(tt, 1);
    sum = _mm_add_ps(sum, _mm_mul_ps(tt, r3));
    __m128 mtL3 = _mm_xor_ps(sum, Sign_PNPN);

    // Dividing is FASTER than rcp_nr! (Because rcp_nr causes many register-memory RWs).
    RDet = _mm_div_ss(_mm_load_ss((float *)&X1_YZ0_W1), Det);
    RDet = _mm_shuffle_ps(RDet, RDet, 0x00);

    // Devide the first 12 minterms with the determinant.
    mtL1 = _mm_mul_ps(mtL1, RDet);
    mtL2 = _mm_mul_ps(mtL2, RDet);
    mtL3 = _mm_mul_ps(mtL3, RDet);

    // Calculate the minterms of the forth line and devide by the determinant.
    tt = sseRor(_L3, 1);
    sum = _mm_mul_ps(tt, r1);
    tt = sseRor(tt, 1);
    sum = _mm_add_ps(sum, _mm_mul_ps(tt, r2));
    tt = sseRor(tt, 1);
    sum = _mm_add_ps(sum, _mm_mul_ps(tt, r3));
    __m128 mtL4 = _mm_xor_ps(sum, Sign_NPNP);
    mtL4 = _mm_mul_ps(mtL4, RDet);

    // Now we just have to transpose the minterms matrix.
    trns0 = _mm_unpacklo_ps(mtL1, mtL2);
    trns1 = _mm_unpacklo_ps(mtL3, mtL4);
    trns2 = _mm_unpackhi_ps(mtL1, mtL2);
    trns3 = _mm_unpackhi_ps(mtL3, mtL4);
    _L1 = _mm_movelh_ps(trns0, trns1);
    _L2 = _mm_movehl_ps(trns1, trns0);
    _L3 = _mm_movelh_ps(trns2, trns3);
    _L4 = _mm_movehl_ps(trns3, trns2);

    return Matrix4(Vector4(_L1), Vector4(_L2), Vector4(_L3), Vector4(_L4));
}

inline const Matrix4 affineInverse(const Matrix4 & mat)
{
    Transform3 affineMat;
    affineMat.setCol0(mat.getCol0().getXYZ());
    affineMat.setCol1(mat.getCol1().getXYZ());
    affineMat.setCol2(mat.getCol2().getXYZ());
    affineMat.setCol3(mat.getCol3().getXYZ());
    return Matrix4(inverse(affineMat));
}

inline const Matrix4 orthoInverse(const Matrix4 & mat)
{
    Transform3 affineMat;
    affineMat.setCol0(mat.getCol0().getXYZ());
    affineMat.setCol1(mat.getCol1().getXYZ());
    affineMat.setCol2(mat.getCol2().getXYZ());
    affineMat.setCol3(mat.getCol3().getXYZ());
    return Matrix4(orthoInverse(affineMat));
}

inline const FloatInVec determinant(const Matrix4 & mat)
{
    __m128 Va, Vb, Vc;
    __m128 r1, r2, r3, tt, tt2;
    __m128 sum, Det;

    __m128 _L1 = mat.getCol0().get128();
    __m128 _L2 = mat.getCol1().get128();
    __m128 _L3 = mat.getCol2().get128();
    __m128 _L4 = mat.getCol3().get128();
    // Calculating the minterms for the first line.

    // sseRor is just a macro using _mm_shuffle_ps().
    tt = _L4;
    tt2 = sseRor(_L3, 1);
    Vc = _mm_mul_ps(tt2, sseRor(tt, 0)); // V3'·V4
    Va = _mm_mul_ps(tt2, sseRor(tt, 2)); // V3'·V4"
    Vb = _mm_mul_ps(tt2, sseRor(tt, 3)); // V3'·V4^

    r1 = _mm_sub_ps(sseRor(Va, 1), sseRor(Vc, 2)); // V3"·V4^ - V3^·V4"
    r2 = _mm_sub_ps(sseRor(Vb, 2), sseRor(Vb, 0)); // V3^·V4' - V3'·V4^
    r3 = _mm_sub_ps(sseRor(Va, 0), sseRor(Vc, 1)); // V3'·V4" - V3"·V4'

    tt = _L2;
    Va = sseRor(tt, 1);
    sum = _mm_mul_ps(Va, r1);
    Vb = sseRor(tt, 2);
    sum = _mm_add_ps(sum, _mm_mul_ps(Vb, r2));
    Vc = sseRor(tt, 3);
    sum = _mm_add_ps(sum, _mm_mul_ps(Vc, r3));

    // Calculating the determinant.
    Det = _mm_mul_ps(sum, _L1);
    Det = _mm_add_ps(Det, _mm_movehl_ps(Det, Det));

    // Calculating the minterms of the second line (using previous results).
    tt = sseRor(_L1, 1);
    sum = _mm_mul_ps(tt, r1);
    tt = sseRor(tt, 1);
    sum = _mm_add_ps(sum, _mm_mul_ps(tt, r2));
    tt = sseRor(tt, 1);
    sum = _mm_add_ps(sum, _mm_mul_ps(tt, r3));

    // Testing the determinant.
    Det = _mm_sub_ss(Det, _mm_shuffle_ps(Det, Det, 1));
    return FloatInVec(Det, 0);
}

inline const Matrix4 Matrix4::operator + (const Matrix4 & mat) const
{
    return Matrix4((mCol0 + mat.mCol0),
                   (mCol1 + mat.mCol1),
                   (mCol2 + mat.mCol2),
                   (mCol3 + mat.mCol3));
}

inline const Matrix4 Matrix4::operator - (const Matrix4 & mat) const
{
    return Matrix4((mCol0 - mat.mCol0),
                   (mCol1 - mat.mCol1),
                   (mCol2 - mat.mCol2),
                   (mCol3 - mat.mCol3));
}

inline Matrix4 & Matrix4::operator += (const Matrix4 & mat)
{
    *this = *this + mat;
    return *this;
}

inline Matrix4 & Matrix4::operator -= (const Matrix4 & mat)
{
    *this = *this - mat;
    return *this;
}

inline const Matrix4 Matrix4::operator - () const
{
    return Matrix4((-mCol0), (-mCol1), (-mCol2), (-mCol3));
}

inline const Matrix4 absPerElem(const Matrix4 & mat)
{
    return Matrix4(absPerElem(mat.getCol0()),
                   absPerElem(mat.getCol1()),
                   absPerElem(mat.getCol2()),
                   absPerElem(mat.getCol3()));
}

inline const Matrix4 Matrix4::operator * (float scalar) const
{
    return *this * FloatInVec(scalar);
}

inline const Matrix4 Matrix4::operator * (const FloatInVec & scalar) const
{
    return Matrix4((mCol0 * scalar),
                   (mCol1 * scalar),
                   (mCol2 * scalar),
                   (mCol3 * scalar));
}

inline Matrix4 & Matrix4::operator *= (float scalar)
{
    return *this *= FloatInVec(scalar);
}

inline Matrix4 & Matrix4::operator *= (const FloatInVec & scalar)
{
    *this = *this * scalar;
    return *this;
}

inline const Matrix4 operator * (float scalar, const Matrix4 & mat)
{
    return FloatInVec(scalar) * mat;
}

inline const Matrix4 operator * (const FloatInVec & scalar, const Matrix4 & mat)
{
    return mat * scalar;
}

inline const Vector4 Matrix4::operator * (const Vector4 & vec) const
{
    return Vector4(
        _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(mCol0.get128(), _mm_shuffle_ps(vec.get128(), vec.get128(), _MM_SHUFFLE(0, 0, 0, 0))), _mm_mul_ps(mCol1.get128(), _mm_shuffle_ps(vec.get128(), vec.get128(), _MM_SHUFFLE(1, 1, 1, 1)))),
            _mm_add_ps(_mm_mul_ps(mCol2.get128(), _mm_shuffle_ps(vec.get128(), vec.get128(), _MM_SHUFFLE(2, 2, 2, 2))), _mm_mul_ps(mCol3.get128(), _mm_shuffle_ps(vec.get128(), vec.get128(), _MM_SHUFFLE(3, 3, 3, 3))))));
}

inline const Vector4 Matrix4::operator * (const Vector3 & vec) const
{
    return Vector4(
        _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(mCol0.get128(), _mm_shuffle_ps(vec.get128(), vec.get128(), _MM_SHUFFLE(0, 0, 0, 0))), _mm_mul_ps(mCol1.get128(), _mm_shuffle_ps(vec.get128(), vec.get128(), _MM_SHUFFLE(1, 1, 1, 1)))),
            _mm_mul_ps(mCol2.get128(), _mm_shuffle_ps(vec.get128(), vec.get128(), _MM_SHUFFLE(2, 2, 2, 2)))));
}

inline const Vector4 Matrix4::operator * (const Point3 & pnt) const
{
    return Vector4(
        _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(mCol0.get128(), _mm_shuffle_ps(pnt.get128(), pnt.get128(), _MM_SHUFFLE(0, 0, 0, 0))), _mm_mul_ps(mCol1.get128(), _mm_shuffle_ps(pnt.get128(), pnt.get128(), _MM_SHUFFLE(1, 1, 1, 1)))),
            _mm_add_ps(_mm_mul_ps(mCol2.get128(), _mm_shuffle_ps(pnt.get128(), pnt.get128(), _MM_SHUFFLE(2, 2, 2, 2))), mCol3.get128())));
}

inline const Matrix4 Matrix4::operator * (const Matrix4 & mat) const
{
    return Matrix4((*this * mat.mCol0),
                   (*this * mat.mCol1),
                   (*this * mat.mCol2),
                   (*this * mat.mCol3));
}

inline Matrix4 & Matrix4::operator *= (const Matrix4 & mat)
{
    *this = *this * mat;
    return *this;
}

inline const Matrix4 Matrix4::operator * (const Transform3 & tfrm) const
{
    return Matrix4((*this * tfrm.getCol0()),
                   (*this * tfrm.getCol1()),
                   (*this * tfrm.getCol2()),
                   (*this * Point3(tfrm.getCol3())));
}

inline Matrix4 & Matrix4::operator *= (const Transform3 & tfrm)
{
    *this = *this * tfrm;
    return *this;
}

inline const Matrix4 mulPerElem(const Matrix4 & mat0, const Matrix4 & mat1)
{
    return Matrix4(mulPerElem(mat0.getCol0(), mat1.getCol0()),
                   mulPerElem(mat0.getCol1(), mat1.getCol1()),
                   mulPerElem(mat0.getCol2(), mat1.getCol2()),
                   mulPerElem(mat0.getCol3(), mat1.getCol3()));
}

inline const Matrix4 Matrix4::identity()
{
    return Matrix4(Vector4::xAxis(),
                   Vector4::yAxis(),
                   Vector4::zAxis(),
                   Vector4::wAxis());
}

inline Matrix4 & Matrix4::setUpper3x3(const Matrix3 & mat3)
{
    mCol0.setXYZ(mat3.getCol0());
    mCol1.setXYZ(mat3.getCol1());
    mCol2.setXYZ(mat3.getCol2());
    return *this;
}

inline const Matrix3 Matrix4::getUpper3x3() const
{
    return Matrix3(mCol0.getXYZ(), mCol1.getXYZ(), mCol2.getXYZ());
}

inline Matrix4 & Matrix4::setTranslation(const Vector3 & translateVec)
{
    mCol3.setXYZ(translateVec);
    return *this;
}

inline const Vector3 Matrix4::getTranslation() const
{
    return mCol3.getXYZ();
}

inline const Matrix4 Matrix4::rotationX(float radians)
{
    return rotationX(FloatInVec(radians));
}

inline const Matrix4 Matrix4::rotationX(const FloatInVec & radians)
{
    __m128 s, c, res1, res2;
    __m128 zero;
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    zero = _mm_setzero_ps();
    sseSinfCosf(radians.get128(), &s, &c);
    res1 = sseSelect(zero, c, select_y);
    res1 = sseSelect(res1, s, select_z);
    res2 = sseSelect(zero, sseNegatef(s), select_y);
    res2 = sseSelect(res2, c, select_z);
    return Matrix4(Vector4::xAxis(),
                   Vector4(res1),
                   Vector4(res2),
                   Vector4::wAxis());
}

inline const Matrix4 Matrix4::rotationY(float radians)
{
    return rotationY(FloatInVec(radians));
}

inline const Matrix4 Matrix4::rotationY(const FloatInVec & radians)
{
    __m128 s, c, res0, res2;
    __m128 zero;
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    zero = _mm_setzero_ps();
    sseSinfCosf(radians.get128(), &s, &c);
    res0 = sseSelect(zero, c, select_x);
    res0 = sseSelect(res0, sseNegatef(s), select_z);
    res2 = sseSelect(zero, s, select_x);
    res2 = sseSelect(res2, c, select_z);
    return Matrix4(Vector4(res0),
                   Vector4::yAxis(),
                   Vector4(res2),
                   Vector4::wAxis());
}

inline const Matrix4 Matrix4::rotationZ(float radians)
{
    return rotationZ(FloatInVec(radians));
}

inline const Matrix4 Matrix4::rotationZ(const FloatInVec & radians)
{
    __m128 s, c, res0, res1;
    __m128 zero;
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    zero = _mm_setzero_ps();
    sseSinfCosf(radians.get128(), &s, &c);
    res0 = sseSelect(zero, c, select_x);
    res0 = sseSelect(res0, s, select_y);
    res1 = sseSelect(zero, sseNegatef(s), select_x);
    res1 = sseSelect(res1, c, select_y);
    return Matrix4(Vector4(res0),
                   Vector4(res1),
                   Vector4::zAxis(),
                   Vector4::wAxis());
}

inline const Matrix4 Matrix4::rotationZYX(const Vector3 & radiansXYZ)
{
    __m128 angles, s, negS, c, X0, X1, Y0, Y1, Z0, Z1, tmp;
    angles = Vector4(radiansXYZ, 0.0f).get128();
    sseSinfCosf(angles, &s, &c);
    negS = sseNegatef(s);
    Z0 = sseMergeL(c, s);
    Z1 = sseMergeL(negS, c);
    VECTORMATH_ALIGNED(unsigned int select_xyz[4]) = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0 };
    Z1 = _mm_and_ps(Z1, _mm_load_ps((float *)select_xyz));
    Y0 = _mm_shuffle_ps(c, negS, _MM_SHUFFLE(0, 1, 1, 1));
    Y1 = _mm_shuffle_ps(s, c, _MM_SHUFFLE(0, 1, 1, 1));
    X0 = sseSplat(s, 0);
    X1 = sseSplat(c, 0);
    tmp = _mm_mul_ps(Z0, Y1);
    return Matrix4(Vector4(_mm_mul_ps(Z0, Y0)),
                   Vector4(sseMAdd(Z1, X1, _mm_mul_ps(tmp, X0))),
                   Vector4(sseMSub(Z1, X0, _mm_mul_ps(tmp, X1))),
                   Vector4::wAxis());
}

inline const Matrix4 Matrix4::rotation(float radians, const Vector3 & unitVec)
{
    return rotation(FloatInVec(radians), unitVec);
}

inline const Matrix4 Matrix4::rotation(const FloatInVec & radians, const Vector3 & unitVec)
{
    __m128 axis, s, c, oneMinusC, axisS, negAxisS, xxxx, yyyy, zzzz, tmp0, tmp1, tmp2;
    axis = unitVec.get128();
    sseSinfCosf(radians.get128(), &s, &c);
    xxxx = sseSplat(axis, 0);
    yyyy = sseSplat(axis, 1);
    zzzz = sseSplat(axis, 2);
    oneMinusC = _mm_sub_ps(_mm_set1_ps(1.0f), c);
    axisS = _mm_mul_ps(axis, s);
    negAxisS = sseNegatef(axisS);
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    tmp0 = _mm_shuffle_ps(axisS, axisS, _MM_SHUFFLE(0, 0, 2, 0));
    tmp0 = sseSelect(tmp0, sseSplat(negAxisS, 1), select_z);
    tmp1 = sseSelect(sseSplat(axisS, 0), sseSplat(negAxisS, 2), select_x);
    tmp2 = _mm_shuffle_ps(axisS, axisS, _MM_SHUFFLE(0, 0, 0, 1));
    tmp2 = sseSelect(tmp2, sseSplat(negAxisS, 0), select_y);
    tmp0 = sseSelect(tmp0, c, select_x);
    tmp1 = sseSelect(tmp1, c, select_y);
    tmp2 = sseSelect(tmp2, c, select_z);
    VECTORMATH_ALIGNED(unsigned int select_xyz[4]) = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0 };
    axis = _mm_and_ps(axis, _mm_load_ps((float *)select_xyz));
    tmp0 = _mm_and_ps(tmp0, _mm_load_ps((float *)select_xyz));
    tmp1 = _mm_and_ps(tmp1, _mm_load_ps((float *)select_xyz));
    tmp2 = _mm_and_ps(tmp2, _mm_load_ps((float *)select_xyz));
    return Matrix4(Vector4(sseMAdd(_mm_mul_ps(axis, xxxx), oneMinusC, tmp0)),
                   Vector4(sseMAdd(_mm_mul_ps(axis, yyyy), oneMinusC, tmp1)),
                   Vector4(sseMAdd(_mm_mul_ps(axis, zzzz), oneMinusC, tmp2)),
                   Vector4::wAxis());
}

inline const Matrix4 Matrix4::rotation(const Quat & unitQuat)
{
    return Matrix4(Transform3::rotation(unitQuat));
}

inline const Matrix4 Matrix4::scale(const Vector3 & scaleVec)
{
    __m128 zero = _mm_setzero_ps();
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    return Matrix4(Vector4(sseSelect(zero, scaleVec.get128(), select_x)),
                   Vector4(sseSelect(zero, scaleVec.get128(), select_y)),
                   Vector4(sseSelect(zero, scaleVec.get128(), select_z)),
                   Vector4::wAxis());
}

inline const Matrix4 appendScale(const Matrix4 & mat, const Vector3 & scaleVec)
{
    return Matrix4((mat.getCol0() * scaleVec.getX()),
                   (mat.getCol1() * scaleVec.getY()),
                   (mat.getCol2() * scaleVec.getZ()),
                   mat.getCol3());
}

inline const Matrix4 prependScale(const Vector3 & scaleVec, const Matrix4 & mat)
{
    const Vector4 scale4 = Vector4(scaleVec, 1.0f);
    return Matrix4(mulPerElem(mat.getCol0(), scale4),
                   mulPerElem(mat.getCol1(), scale4),
                   mulPerElem(mat.getCol2(), scale4),
                   mulPerElem(mat.getCol3(), scale4));
}

inline const Matrix4 Matrix4::translation(const Vector3 & translateVec)
{
    return Matrix4(Vector4::xAxis(),
                   Vector4::yAxis(),
                   Vector4::zAxis(),
                   Vector4(translateVec, 1.0f));
}

inline const Matrix4 Matrix4::lookAtLH(const Point3 & eyePos, const Point3 & lookAtPos, const Vector3 & upVec)
{
	Matrix4 m4EyeFrame;
	Vector3 v3X, v3Y, v3Z;
	v3Y = normalize(upVec);
	v3Z = normalize(lookAtPos - eyePos);
	v3X = normalize(cross(v3Y, v3Z));
	v3Y = cross(v3Z, v3X);
	m4EyeFrame = Matrix4(Vector4(v3X), Vector4(v3Y), Vector4(v3Z), Vector4(eyePos));
	return orthoInverse(m4EyeFrame);
}

inline const Matrix4 Matrix4::lookAtRH(const Point3 & eyePos, const Point3 & lookAtPos, const Vector3 & upVec)
{
    Matrix4 m4EyeFrame;
    Vector3 v3X, v3Y, v3Z;
    v3Y = normalize(upVec);
    v3Z = normalize(eyePos - lookAtPos);
    v3X = normalize(cross(v3Y, v3Z));
    v3Y = cross(v3Z, v3X);
    m4EyeFrame = Matrix4(Vector4(v3X), Vector4(v3Y), Vector4(v3Z), Vector4(eyePos));
    return orthoInverse(m4EyeFrame);
}

inline const Matrix4 Matrix4::frustumLH(float left, float right, float bottom, float top, float zNear, float zFar)
{
    /* function implementation based on code from STIDC SDK:           */
    /* --------------------------------------------------------------  */
    /* PLEASE DO NOT MODIFY THIS SECTION                               */
    /* This prolog section is automatically generated.                 */
    /*                                                                 */
    /* (C)Copyright                                                    */
    /* Sony Computer Entertainment, Inc.,                              */
    /* Toshiba Corporation,                                            */
    /* International Business Machines Corporation,                    */
    /* 2001,2002.                                                      */
    /* S/T/I Confidential Information                                  */
    /* --------------------------------------------------------------  */
    __m128 lbf, rtn;
    __m128 diff, inv_diff;
    __m128 diagonalXY, diagonalZ, column, near1, near2;
    __m128 zero = _mm_setzero_ps();
    SSEFloat l, f, r, n, b, t;
    l.f[0] = left;
    f.f[0] = zFar;
    r.f[0] = right;
    n.f[0] = zNear;
    b.f[0] = bottom;
    t.f[0] = top;
    lbf = sseMergeH(l.m128, f.m128);
    rtn = sseMergeH(r.m128, n.m128);
    lbf = sseMergeH(lbf, b.m128);
    rtn = sseMergeH(rtn, t.m128);
    diff = _mm_sub_ps(rtn, lbf);
    inv_diff = sseRecipf(diff);
    near1 = sseSplat(n.m128, 0);
	near2 = _mm_add_ps(near1, near1);
    diagonalXY = _mm_mul_ps(near2, inv_diff);
	diagonalZ = _mm_mul_ps(near1, inv_diff);
	column = _mm_mul_ps(lbf, inv_diff);
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
	VECTORMATH_ALIGNED(unsigned int select_xyw[4]) = { 0xFFFFFFFF, 0xFFFFFFFF, 0, 0xFFFFFFFF };
	Matrix4 m = Matrix4(Vector4(sseSelect(zero, diagonalXY, select_x)),
                   Vector4(sseSelect(zero, diagonalXY, select_y)),
                   Vector4(sseSelect(column, _mm_set_ps(-1.0f, INFINITY, 0.0f, 0.0f), select_xyw)),
                   Vector4(sseSelect(zero, _mm_mul_ps(diagonalZ, sseSplat(f.m128, 0)), select_z)));
	m.setCol2(-m.getCol2()); // so far we've constructed RH matrix, make it LH
	return m;
}

//========================================= #TheForgeMathExtensionsBegin ================================================
// Note: If math library is updated, remember to add the below functions. search for #TheForgeMathExtensions

// The default constructor of mat4 uses the vec4 arguments as columns.
// This macro maps the default notation like you see on paper to this constructor.
//Remember to update the documentation when this macro name is changed
#define CONSTRUCT_TRANSPOSED_MAT4(m11,m12,m13,m14,m21,m22,m23,m24,m31,m32,m33,m34,m41,m42,m43,m44) \
	Matrix4(Vector4(m11,m21,m31,m41),\
		 Vector4(m12,m22,m32,m42),\
		 Vector4(m13,m23,m33,m43),\
		 Vector4(m14,m24,m34,m44))

#define POSITIVE_X 0
#define NEGATIVE_X 1
#define POSITIVE_Y 2
#define NEGATIVE_Y 3
#define POSITIVE_Z 4
#define NEGATIVE_Z 5
//----------------------------------------------------------------------------------------

#define USE_VERTICAL_FIELD_OF_VIEW 0	// The Forge uses perspective() with horizontal field of view, defined below

inline float FovHorizontalToVertical(float fovX, float aspectInverse)
{
	return 2.0f * atanf(tanf(fovX / 2.0f) * aspectInverse);
}

inline float FovVerticalToHorizontal(float fovY, float aspectInverse)
{
	return 2.0f * atanf(tanf(fovY / 2.0f) / aspectInverse);
}

inline const Matrix4 Matrix4::perspectiveLH(float fovRadians, float aspectInverse, float zNear, float zFar)
{
	static const float VECTORMATH_PI_OVER_2 = 1.570796327f;

	float f, rangeInv;
	SSEFloat tmp;
	__m128 col0, col1, col2, col3;

#if USE_VERTICAL_FIELD_OF_VIEW
	fovRadians = FovVerticalToHorizontal(fovRadians, aspectInverse);
#endif

#if defined(__linux__)
// linux build uses c++11 standard
	f = std::tan(VECTORMATH_PI_OVER_2 - fovRadians * 0.5f);
#else
	f = ::tanf(VECTORMATH_PI_OVER_2 - fovRadians * 0.5f);
#endif

	rangeInv = 1.0f / (zFar - zNear);

	const __m128 zero = _mm_setzero_ps();
	tmp.m128 = zero;
	tmp.f[0] = f;
	col0 = tmp.m128;
	tmp.m128 = zero;
	tmp.f[1] = f / aspectInverse;
	col1 = tmp.m128;
	tmp.m128 = zero;
	tmp.f[2] = zFar * rangeInv;
    tmp.f[3] = +1.0f;
	col2 = tmp.m128;
	tmp.m128 = zero;
	tmp.f[2] = -zNear * zFar * rangeInv;
	col3 = tmp.m128;

	return Matrix4(Vector4(col0), Vector4(col1), Vector4(col2), Vector4(col3));
}
inline const Matrix4 Matrix4::perspectiveRH(float fovRadians, float aspectInverse, float zNear, float zFar)
{
	Matrix4 m = perspectiveLH(fovRadians, aspectInverse, zNear, zFar);
	m.setCol2(-m.getCol2());
	return m;
}

inline const Matrix4 Matrix4::perspectiveLH_ReverseZ(float fovRadians, float aspectInverse, float zNear, float zFar)
{
	Matrix4 perspMatrix = perspectiveLH(fovRadians, aspectInverse, zNear, zFar);
	
	const Vector4 &col2 = perspMatrix.mCol2;
	const Vector4 &col3 = perspMatrix.mCol3;
	perspMatrix.mCol2.setZ(col2.getW() - col2.getZ());
	perspMatrix.mCol3.setZ(-col3.getZ());
	
	return perspMatrix;
}

inline const Matrix4 Matrix4::orthographicLH(float left, float right, float bottom, float top, float zNear, float zFar)
{	
	__m128 lbn, rtf;
	__m128 diff, sum, inv_diff, neg_inv_diff;
	__m128 diagonal, column;
	__m128 zero = _mm_setzero_ps();
	__m128 l, f, r, n, b, t;
	l = _mm_set_ps1(left);
	f = _mm_set_ps1(zFar);
	r = _mm_set_ps1(right);
	n = _mm_set_ps1(zNear);
	b = _mm_set_ps1(bottom);
	t = _mm_set_ps1(top);
	lbn = sseMergeH(l, n);
	rtf = sseMergeH(r, f);
	lbn = sseMergeH(lbn, b);
	rtf = sseMergeH(rtf, t);
	diff = _mm_sub_ps(rtf, lbn);
	inv_diff = sseRecipf(diff);
	neg_inv_diff = sseNegatef(inv_diff);
	VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
	VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
	VECTORMATH_ALIGNED(unsigned int select_w[4]) = { 0, 0, 0, 0xFFFFFFFF };
	sum = _mm_add_ps(rtf, sseSelect(lbn, _mm_sub_ps(n, f), select_z));
	diagonal = _mm_add_ps(inv_diff, sseSelect(inv_diff, zero, select_z));
	column = _mm_mul_ps(sum, neg_inv_diff);
	return Matrix4( Vector4(sseSelect(zero, diagonal, select_x)),
					Vector4(sseSelect(zero, diagonal, select_y)),
					Vector4(sseSelect(zero, diagonal, select_z)),
					Vector4(sseSelect(column, _mm_set1_ps(1.0f), select_w)));
}

inline const Matrix4 Matrix4::orthographicRH(float left, float right, float bottom, float top, float zNear, float zFar)
{
	Matrix4 m = orthographicLH(left, right, bottom, top, zNear, zFar);
	m.setCol2(-m.getCol2());
	return m;
}

inline const Matrix4 Matrix4::orthographicLH_ReverseZ(float left, float right, float bottom, float top, float zNear, float zFar)
{
	Matrix4 orthoMatrix = orthographicLH(left, right, bottom, top, zNear, zFar);

	const Vector4 &col2 = orthoMatrix.mCol2;
	const Vector4 &col3 = orthoMatrix.mCol3;
	orthoMatrix.mCol2.setZ(-col2.getZ());
	orthoMatrix.mCol3.setZ(-col3.getZ() * zFar / zNear);

	return orthoMatrix;
}

inline const Matrix4 Matrix4::cubeProjectionLH(const float zNear, const float zFar)
{
	return CONSTRUCT_TRANSPOSED_MAT4(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, zFar / (zFar - zNear), (zFar * zNear) / (zNear - zFar),
		0, 0, 1, 0);
}
inline const Matrix4 Matrix4::cubeProjectionRH(const float zNear, const float zFar)
{
	Matrix4 m = cubeProjectionLH(zNear, zFar);
	m.setCol2(-m.getCol2());
	return m;
}

inline const Matrix4 Matrix4::cubeView(const unsigned int side)
{
	switch (side)
	{
	case POSITIVE_X:
		return CONSTRUCT_TRANSPOSED_MAT4(
			0, 0, -1, 0,
			0, 1, 0, 0,
			1, 0, 0, 0,
			0, 0, 0, 1);
	case NEGATIVE_X:
		return CONSTRUCT_TRANSPOSED_MAT4(
			0, 0, 1, 0,
			0, 1, 0, 0,
			-1, 0, 0, 0,
			0, 0, 0, 1);
	case POSITIVE_Y:
		return CONSTRUCT_TRANSPOSED_MAT4(
			1, 0, 0, 0,
			0, 0, -1, 0,
			0, 1, 0, 0,
			0, 0, 0, 1);
	case NEGATIVE_Y:
		return CONSTRUCT_TRANSPOSED_MAT4(
			1, 0, 0, 0,
			0, 0, 1, 0,
			0, -1, 0, 0,
			0, 0, 0, 1);
	case POSITIVE_Z:
		return CONSTRUCT_TRANSPOSED_MAT4(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);
		//case NEGATIVE_Z:
	default:
		return CONSTRUCT_TRANSPOSED_MAT4(
			-1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, -1, 0,
			0, 0, 0, 1);
	}
}

inline void Matrix4::extractFrustumClipPlanes(const Matrix4& vp, Vector4& rcp, Vector4& lcp, Vector4& tcp, Vector4& bcp, Vector4& fcp, Vector4& ncp, bool const normalizePlanes)
{
	// Left plane
	lcp = vp.getRow(3) + vp.getRow(0);

	// Right plane
	rcp = vp.getRow(3) - vp.getRow(0);

	// Bottom plane
	bcp = vp.getRow(3) + vp.getRow(1);

	// Top plane
	tcp = vp.getRow(3) - vp.getRow(1);

	// Near plane
	ncp = vp.getRow(3) + vp.getRow(2);

	// Far plane
	fcp = vp.getRow(3) - vp.getRow(2);

	// Normalize if needed
	if (normalizePlanes)
	{
		float lcp_norm = length(lcp.getXYZ());
		lcp /= lcp_norm;

		float rcp_norm = length(rcp.getXYZ());
		rcp /= rcp_norm;

		float bcp_norm = length(bcp.getXYZ());
		bcp /= bcp_norm;

		float tcp_norm = length(tcp.getXYZ());
		tcp /= tcp_norm;

		float ncp_norm = length(ncp.getXYZ());
		ncp /= ncp_norm;

		float fcp_norm = length(fcp.getXYZ());
		fcp /= fcp_norm;
	}
}

inline const Matrix4 Matrix4::rotationYX(const float radiansY, const float radiansX)
{
	// Note that:
	//  rotateYX(-y,-x)*rotateXY(x,y) == mat4::identity()
	// which means that
	//  inverse(rotateXY(x,y)) = rotateYX(-y,-x)

	//return mat4::rotationY(angleY) * mat4::rotationX(angleX);
	/*
	[	1,		 0,		 0,		 0]
	[	0,		 c,		-s,		 0]
	[	0,		 s,		 c,		 0]
	[	0,		 0,		 0,		 1]

	[ o, 0, i, 0]	[   o,		is,		ic,		0]
	[ 0, 1, 0, 0]	[   0,		c ,		-s,		0]
	[-i, 0, o, 0]	[  -i,		os,		oc,		0]
	[ 0, 0, 0, 1]	[   0,		0 ,		0 ,		1]
	*/
	const float cosX = cosf(radiansX), sinX = sinf(radiansX);
	const float cosY = cosf(radiansY), sinY = sinf(radiansY);
	return CONSTRUCT_TRANSPOSED_MAT4(
		cosY, sinY*sinX, sinY*cosX, 0,
		0, cosX, -sinX, 0,
		-sinY, cosY*sinX, cosY*cosX, 0,
		0, 0, 0, 1
	);
}

inline const Matrix4 Matrix4::rotationXY(const float radiansX, const float radiansY)
{	//same as: return mat4::rotationX(angleX) * mat4::rotationY(angleY);
	const float cosX = cosf(radiansX), sinX = sinf(radiansX);
	const float cosY = cosf(radiansY), sinY = sinf(radiansY);

	return CONSTRUCT_TRANSPOSED_MAT4(
		cosY, 0, sinY, 0,
		sinX * sinY, cosX, -sinX * cosY, 0,
		cosX *-sinY, sinX, cosX * cosY, 0,
		0, 0, 0, 1);
}

//========================================= #TheForgeMathExtensionsEnd ==================================================


inline const Matrix4 select(const Matrix4 & mat0, const Matrix4 & mat1, bool select1)
{
    return Matrix4(select(mat0.getCol0(), mat1.getCol0(), select1),
                   select(mat0.getCol1(), mat1.getCol1(), select1),
                   select(mat0.getCol2(), mat1.getCol2(), select1),
                   select(mat0.getCol3(), mat1.getCol3(), select1));
}

inline const Matrix4 select(const Matrix4 & mat0, const Matrix4 & mat1, const BoolInVec & select1)
{
    return Matrix4(select(mat0.getCol0(), mat1.getCol0(), select1),
                   select(mat0.getCol1(), mat1.getCol1(), select1),
                   select(mat0.getCol2(), mat1.getCol2(), select1),
                   select(mat0.getCol3(), mat1.getCol3(), select1));
}

#ifdef VECTORMATH_DEBUG

inline void print(const Matrix4 & mat)
{
    print(mat.getRow(0));
    print(mat.getRow(1));
    print(mat.getRow(2));
    print(mat.getRow(3));
}

inline void print(const Matrix4 & mat, const char * name)
{
    std::printf("%s:\n", name);
    print(mat);
}

#endif // VECTORMATH_DEBUG

// ========================================================
// Matrix4d
// ========================================================

inline Matrix4d::Matrix4d(const Matrix4d & mat)
{
	mCol0 = mat.mCol0;
	mCol1 = mat.mCol1;
	mCol2 = mat.mCol2;
	mCol3 = mat.mCol3;
}

inline Matrix4d::Matrix4d(double scalar)
{
	mCol0 = Vector4d(scalar);
	mCol1 = Vector4d(scalar);
	mCol2 = Vector4d(scalar);
	mCol3 = Vector4d(scalar);
}

inline Matrix4d::Matrix4d(const DoubleInVec & scalar)
{
	mCol0 = Vector4d(scalar);
	mCol1 = Vector4d(scalar);
	mCol2 = Vector4d(scalar);
	mCol3 = Vector4d(scalar);
}

inline Matrix4d::Matrix4d(const Transform3 & mat)
{
	SSEFloat c0, c1, c2, c3;
	c0.m128 = mat.getCol0().get128();
	c1.m128 = mat.getCol1().get128();
	c2.m128 = mat.getCol2().get128();
	c3.m128 = mat.getCol3().get128();
	mCol0 = Vector4d(dsseFromFVec4(c0));
	mCol1 = Vector4d(dsseFromFVec4(c1));
	mCol2 = Vector4d(dsseFromFVec4(c2));
	mCol3 = Vector4d(dsseFromFVec4(c3));
}

inline Matrix4d::Matrix4d(const Vector4d & _col0, const Vector4d & _col1, const Vector4d & _col2, const Vector4d & _col3)
{
	mCol0 = _col0;
	mCol1 = _col1;
	mCol2 = _col2;
	mCol3 = _col3;
}

inline Matrix4d::Matrix4d(const Matrix3d & mat, const Vector3d & translateVec)
{
	mCol0 = Vector4d(mat.getCol0(), 0.0);
	mCol1 = Vector4d(mat.getCol1(), 0.0);
	mCol2 = Vector4d(mat.getCol2(), 0.0);
	mCol3 = Vector4d(translateVec,  1.0);
}

inline Matrix4d::Matrix4d(const Quat & unitQuat, const Vector3d & translateVec)
{
	Matrix3d mat;
	mat = Matrix3d(unitQuat);
	mCol0 = Vector4d(mat.getCol0(), 0.0);
	mCol1 = Vector4d(mat.getCol1(), 0.0);
	mCol2 = Vector4d(mat.getCol2(), 0.0);
	mCol3 = Vector4d(translateVec,  1.0);
}

inline Matrix4d & Matrix4d::setCol0(const Vector4d & _col0)
{
	mCol0 = _col0;
	return *this;
}

inline Matrix4d & Matrix4d::setCol1(const Vector4d & _col1)
{
	mCol1 = _col1;
	return *this;
}

inline Matrix4d & Matrix4d::setCol2(const Vector4d & _col2)
{
	mCol2 = _col2;
	return *this;
}

inline Matrix4d & Matrix4d::setCol3(const Vector4d & _col3)
{
	mCol3 = _col3;
	return *this;
}

inline Matrix4d & Matrix4d::setCol(int col, const Vector4d & vec)
{
	*(&mCol0 + col) = vec;
	return *this;
}

inline Matrix4d & Matrix4d::setRow(int row, const Vector4d & vec)
{
	mCol0.setElem(row, vec.getElem(0));
	mCol1.setElem(row, vec.getElem(1));
	mCol2.setElem(row, vec.getElem(2));
	mCol3.setElem(row, vec.getElem(3));
	return *this;
}

inline Matrix4d & Matrix4d::setElem(int col, int row, double val)
{
	(*this)[col].setElem(row, val);
	return *this;
}

inline Matrix4d & Matrix4d::setElem(int col, int row, const DoubleInVec & val)
{
	Vector4d tmpV3_0;
	tmpV3_0 = this->getCol(col);
	tmpV3_0.setElem(row, val);
	this->setCol(col, tmpV3_0);
	return *this;
}

inline const DoubleInVec Matrix4d::getElem(int col, int row) const
{
	return this->getCol(col).getElem(row);
}

inline const Vector4d Matrix4d::getCol0() const
{
	return mCol0;
}

inline const Vector4d Matrix4d::getCol1() const
{
	return mCol1;
}

inline const Vector4d Matrix4d::getCol2() const
{
	return mCol2;
}

inline const Vector4d Matrix4d::getCol3() const
{
	return mCol3;
}

inline const Vector4d Matrix4d::getCol(int col) const
{
	return *(&mCol0 + col);
}

inline const Vector4d Matrix4d::getRow(int row) const
{
	return Vector4d(mCol0.getElem(row), mCol1.getElem(row), mCol2.getElem(row), mCol3.getElem(row));
}

inline Vector4d & Matrix4d::operator[](int col)
{
	return *(&mCol0 + col);
}

inline const Vector4d Matrix4d::operator[](int col) const
{
	return *(&mCol0 + col);
}

inline Matrix4d & Matrix4d::operator = (const Matrix4d & mat)
{
	mCol0 = mat.mCol0;
	mCol1 = mat.mCol1;
	mCol2 = mat.mCol2;
	mCol3 = mat.mCol3;
	return *this;
}

inline const Matrix4d transpose(const Matrix4d & mat)
{
	DSSEVec4 tmp0, tmp1, tmp2, tmp3, res0, res1, res2, res3;
	tmp0 = dsseMergeH(mat.getCol0().get256(), mat.getCol2().get256());
	tmp1 = dsseMergeH(mat.getCol1().get256(), mat.getCol3().get256());
	tmp2 = dsseMergeL(mat.getCol0().get256(), mat.getCol2().get256());
	tmp3 = dsseMergeL(mat.getCol1().get256(), mat.getCol3().get256());
	res0 = dsseMergeH(tmp0, tmp1);
	res1 = dsseMergeL(tmp0, tmp1);
	res2 = dsseMergeH(tmp2, tmp3);
	res3 = dsseMergeL(tmp2, tmp3);
	return Matrix4d(Vector4d(res0), Vector4d(res1), Vector4d(res2), Vector4d(res3));
}

inline const Matrix4d inverse(const Matrix4d & mat)
{
	VECTORMATH_ALIGNED(unsigned long long PNPN[4]) = { 0x00000000, 0x8000000000000000, 0x00000000, 0x8000000000000000 };
	VECTORMATH_ALIGNED(unsigned long long NPNP[4]) = { 0x8000000000000000, 0x00000000, 0x8000000000000000, 0x00000000 };
	VECTORMATH_ALIGNED(double X1_YZ0_W1[4])   = { 1.0f, 0.0f, 0.0f, 1.0f };

	DSSEVec4 Va, Vb, Vc;
	DSSEVec4 r1, r2, r3, tt, tt2;
	DSSEVec4 sum, Det, RDet = dsseSetZero();
	DSSEVec4 trns0, trns1, trns2, trns3;

	DSSEVec4 _L1 = mat.getCol0().get256();
	DSSEVec4 _L2 = mat.getCol1().get256();
	DSSEVec4 _L3 = mat.getCol2().get256();
	DSSEVec4 _L4 = mat.getCol3().get256();
	// Calculating the minterms for the first line.

	// sseRor is just a macro using _mm_shuffle_ps().
	tt = _L4;
	tt2 = dsseRor(_L3, 1);
	Vc = dsseMul(tt2, dsseRor(tt, 0)); // V3'·V4
	Va = dsseMul(tt2, dsseRor(tt, 2)); // V3'·V4"
	Vb = dsseMul(tt2, dsseRor(tt, 3)); // V3'·V4^

	r1 = dsseSub(dsseRor(Va, 1), dsseRor(Vc, 2)); // V3"·V4^ - V3^·V4"
	r2 = dsseSub(dsseRor(Vb, 2), dsseRor(Vb, 0)); // V3^·V4' - V3'·V4^
	r3 = dsseSub(dsseRor(Va, 0), dsseRor(Vc, 1)); // V3'·V4" - V3"·V4'

	tt = _L2;
	Va = dsseRor(tt, 1);
	sum = dsseMul(Va, r1);
	Vb = dsseRor(tt, 2);
	sum = dsseAdd(sum, dsseMul(Vb, r2));
	Vc = dsseRor(tt, 3);
	sum = dsseAdd(sum, dsseMul(Vc, r3));

	// Calculating the determinant.
	Det = dsseMul(sum, _L1);
	Det = dsseAdd(Det, dsseMoveHL(Det, Det));

	const DSSEVec4 Sign_PNPN = dsseLoadu((double *)PNPN);
	const DSSEVec4 Sign_NPNP = dsseLoadu((double *)NPNP);

	DSSEVec4 mtL1 = dsseXor(sum, Sign_PNPN);

	// Calculating the minterms of the second line (using previous results).
	tt = dsseRor(_L1, 1);
	sum = dsseMul(tt, r1);
	tt = dsseRor(tt, 1);
	sum = dsseAdd(sum, dsseMul(tt, r2));
	tt = dsseRor(tt, 1);
	sum = dsseAdd(sum, dsseMul(tt, r3));
	DSSEVec4 mtL2 = dsseXor(sum, Sign_NPNP);

	// Testing the determinant.
	Det.xy = _mm_sub_sd(Det.xy, dsseShuffle(Det, Det, 1).xy);

	// Calculating the minterms of the third line.
	tt = dsseRor(_L1, 1);
	Va = dsseMul(tt, Vb);  // V1'·V2"
	Vb = dsseMul(tt, Vc);  // V1'·V2^
	Vc = dsseMul(tt, _L2); // V1'·V2

	r1 = dsseSub(dsseRor(Va, 1), dsseRor(Vc, 2)); // V1"·V2^ - V1^·V2"
	r2 = dsseSub(dsseRor(Vb, 2), dsseRor(Vb, 0)); // V1^·V2' - V1'·V2^
	r3 = dsseSub(dsseRor(Va, 0), dsseRor(Vc, 1)); // V1'·V2" - V1"·V2'

	tt = dsseRor(_L4, 1);
	sum = dsseMul(tt, r1);
	tt = dsseRor(tt, 1);
	sum = dsseAdd(sum, dsseMul(tt, r2));
	tt = dsseRor(tt, 1);
	sum = dsseAdd(sum, dsseMul(tt, r3));
	DSSEVec4 mtL3 = dsseXor(sum, Sign_PNPN);

	// Dividing is FASTER than rcp_nr! (Because rcp_nr causes many register-memory RWs).
	RDet.xy = _mm_div_sd(_mm_load_sd((double *)&X1_YZ0_W1), Det.xy);
	RDet = dsseShuffle(RDet, RDet, 0x00);

	// Devide the first 12 minterms with the determinant.
	mtL1 = dsseMul(mtL1, RDet);
	mtL2 = dsseMul(mtL2, RDet);
	mtL3 = dsseMul(mtL3, RDet);

	// Calculate the minterms of the forth line and devide by the determinant.
	tt = dsseRor(_L3, 1);
	sum = dsseMul(tt, r1);
	tt = dsseRor(tt, 1);
	sum = dsseAdd(sum, dsseMul(tt, r2));
	tt = dsseRor(tt, 1);
	sum = dsseAdd(sum, dsseMul(tt, r3));
	DSSEVec4 mtL4 = dsseXor(sum, Sign_NPNP);
	mtL4 = dsseMul(mtL4, RDet);

	// Now we just have to transpose the minterms matrix.
	trns0 = dsseMergeH(mtL1, mtL2);
	trns1 = dsseMergeH(mtL3, mtL4);
	trns2 = dsseMergeL(mtL1, mtL2);
	trns3 = dsseMergeL(mtL3, mtL4);
	_L1 = dsseMoveLH(trns0, trns1);
	_L2 = dsseMoveHL(trns1, trns0);
	_L3 = dsseMoveLH(trns2, trns3);
	_L4 = dsseMoveHL(trns3, trns2);

	return Matrix4d(Vector4d(_L1), Vector4d(_L2), Vector4d(_L3), Vector4d(_L4));
}

inline const Matrix4d affineInverse(const Matrix4d & mat)
{
	DSSEVec4 inv0, inv1, inv2, inv3;
	DSSEVec4 tmp0, tmp1, tmp2, tmp3, tmp4, dot, invdet;
	DSSEVec4 xxxx, yyyy, zzzz;
	tmp2 = dsseVecCross(mat.getCol0().get256(), mat.getCol1().get256());
	tmp0 = dsseVecCross(mat.getCol1().get256(), mat.getCol2().get256());
	tmp1 = dsseVecCross(mat.getCol2().get256(), mat.getCol0().get256());
	inv3 = dsseNegatef(mat.getCol3().get256());
	dot = dsseVecDot3(tmp2, mat.getCol2().get256());
	dot = dsseSplat(dot, 0);
	invdet = dsseRecipf(dot);
	tmp3 = dsseMergeH(tmp0, tmp2);
	tmp4 = dsseMergeL(tmp0, tmp2);
	inv0 = dsseMergeH(tmp3, tmp1);
	xxxx = dsseSplat(inv3, 0);
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	inv1 = dsseShuffle(tmp3, tmp3, _MM_SHUFFLE(0, 3, 2, 2));
	inv1 = dsseSelect(inv1, tmp1, select_y);
	inv2 = dsseShuffle(tmp4, tmp4, _MM_SHUFFLE(0, 1, 1, 0));
	inv2 = dsseSelect(inv2, dsseSplat(tmp1, 2), select_y);
	yyyy = dsseSplat(inv3, 1);
	zzzz = dsseSplat(inv3, 2);
	inv3 = dsseMul(inv0, xxxx);
	inv3 = dsseMAdd(inv1, yyyy, inv3);
	inv3 = dsseMAdd(inv2, zzzz, inv3);
	inv0 = dsseMul(inv0, invdet);
	inv1 = dsseMul(inv1, invdet);
	inv2 = dsseMul(inv2, invdet);
	inv3 = dsseMul(inv3, invdet);
	return Matrix4d(Matrix3d(Vector3d(inv0), Vector3d(inv1), Vector3d(inv2)), Vector3d(inv3));
}

inline const Matrix4d orthoInverse(const Matrix4d & mat)
{
	DSSEVec4 inv0, inv1, inv2, inv3;
	DSSEVec4 tmp0, tmp1;
	DSSEVec4 xxxx, yyyy, zzzz;
	tmp0 = dsseMergeH(mat.getCol0().get256(), mat.getCol2().get256());
	tmp1 = dsseMergeL(mat.getCol0().get256(), mat.getCol2().get256());
	inv3 = dsseNegatef(mat.getCol3().get256());
	inv0 = dsseMergeH(tmp0, mat.getCol1().get256());
	xxxx = dsseSplat(inv3, 0);
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	inv1 = dsseShuffle(tmp0, tmp0, _MM_SHUFFLE(0, 3, 2, 2));
	inv1 = dsseSelect(inv1, mat.getCol1().get256(), select_y);
	inv2 = dsseShuffle(tmp1, tmp1, _MM_SHUFFLE(0, 1, 1, 0));
	inv2 = dsseSelect(inv2, dsseSplat(mat.getCol1().get256(), 2), select_y);
	yyyy = dsseSplat(inv3, 1);
	zzzz = dsseSplat(inv3, 2);
	inv3 = dsseMul(inv0, xxxx);
	inv3 = dsseMAdd(inv1, yyyy, inv3);
	inv3 = dsseMAdd(inv2, zzzz, inv3);
	return Matrix4d(Matrix3d(Vector3d(inv0), Vector3d(inv1), Vector3d(inv2)), Vector3d(inv3));
}

inline const DoubleInVec determinant(const Matrix4d & mat)
{
	DSSEVec4 Va, Vb, Vc;
	DSSEVec4 r1, r2, r3, tt, tt2;
	DSSEVec4 sum, Det;

	DSSEVec4 _L1 = mat.getCol0().get256();
	DSSEVec4 _L2 = mat.getCol1().get256();
	DSSEVec4 _L3 = mat.getCol2().get256();
	DSSEVec4 _L4 = mat.getCol3().get256();
	// Calculating the minterms for the first line.

	// sseRor is just a macro using _mm_shuffle_ps().
	tt = _L4;
	tt2 = dsseRor(_L3, 1);
	Vc = dsseMul(tt2, dsseRor(tt, 0)); // V3'·V4
	Va = dsseMul(tt2, dsseRor(tt, 2)); // V3'·V4"
	Vb = dsseMul(tt2, dsseRor(tt, 3)); // V3'·V4^

	r1 = dsseSub(dsseRor(Va, 1), dsseRor(Vc, 2)); // V3"·V4^ - V3^·V4"
	r2 = dsseSub(dsseRor(Vb, 2), dsseRor(Vb, 0)); // V3^·V4' - V3'·V4^
	r3 = dsseSub(dsseRor(Va, 0), dsseRor(Vc, 1)); // V3'·V4" - V3"·V4'

	tt = _L2;
	Va = dsseRor(tt, 1);
	sum = dsseMul(Va, r1);
	Vb = dsseRor(tt, 2);
	sum = dsseAdd(sum, dsseMul(Vb, r2));
	Vc = dsseRor(tt, 3);
	sum = dsseAdd(sum, dsseMul(Vc, r3));

	// Calculating the determinant.
	Det = dsseMul(sum, _L1);
	Det = dsseAdd(Det, dsseMoveHL(Det, Det));

	// Calculating the minterms of the second line (using previous results).
	tt = dsseRor(_L1, 1);
	sum = dsseMul(tt, r1);
	tt = dsseRor(tt, 1);
	sum = dsseAdd(sum, dsseMul(tt, r2));
	tt = dsseRor(tt, 1);
	sum = dsseAdd(sum, dsseMul(tt, r3));

	// Testing the determinant.
	Det = dsseSub(Det, dsseShuffle(Det, Det, 1));
	return DoubleInVec(Det, 0);
}

inline const Matrix4d Matrix4d::operator + (const Matrix4d & mat) const
{
	return Matrix4d((mCol0 + mat.mCol0),
				   (mCol1 + mat.mCol1),
				   (mCol2 + mat.mCol2),
				   (mCol3 + mat.mCol3));
}

inline const Matrix4d Matrix4d::operator - (const Matrix4d & mat) const
{
	return Matrix4d((mCol0 - mat.mCol0),
				   (mCol1 - mat.mCol1),
				   (mCol2 - mat.mCol2),
				   (mCol3 - mat.mCol3));
}

inline Matrix4d & Matrix4d::operator += (const Matrix4d & mat)
{
	*this = *this + mat;
	return *this;
}

inline Matrix4d & Matrix4d::operator -= (const Matrix4d & mat)
{
	*this = *this - mat;
	return *this;
}

inline const Matrix4d Matrix4d::operator - () const
{
	return Matrix4d((-mCol0), (-mCol1), (-mCol2), (-mCol3));
}

inline const Matrix4d absPerElem(const Matrix4d & mat)
{
	return Matrix4d(absPerElem(mat.getCol0()),
				   absPerElem(mat.getCol1()),
				   absPerElem(mat.getCol2()),
				   absPerElem(mat.getCol3()));
}

inline const Matrix4d Matrix4d::operator * (double scalar) const
{
	return *this * DoubleInVec(scalar);
}

inline const Matrix4d Matrix4d::operator * (const DoubleInVec & scalar) const
{
	return Matrix4d((mCol0 * scalar),
				   (mCol1 * scalar),
				   (mCol2 * scalar),
				   (mCol3 * scalar));
}

inline Matrix4d & Matrix4d::operator *= (double scalar)
{
	return *this *= DoubleInVec(scalar);
}

inline Matrix4d & Matrix4d::operator *= (const DoubleInVec & scalar)
{
	*this = *this * scalar;
	return *this;
}

inline const Matrix4d operator * (double scalar, const Matrix4d & mat)
{
	return DoubleInVec(scalar) * mat;
}

inline const Matrix4d operator * (const DoubleInVec & scalar, const Matrix4d & mat)
{
	return mat * scalar;
}

inline const Vector4d Matrix4d::operator * (const Vector4d & vec) const
{
	return Vector4d(
		dsseAdd(
			dsseAdd(dsseMul(mCol0.get256(), dsseShuffle(vec.get256(), vec.get256(), _MM_SHUFFLE(0, 0, 0, 0))), dsseMul(mCol1.get256(), dsseShuffle(vec.get256(), vec.get256(), _MM_SHUFFLE(1, 1, 1, 1)))),
			dsseAdd(dsseMul(mCol2.get256(), dsseShuffle(vec.get256(), vec.get256(), _MM_SHUFFLE(2, 2, 2, 2))), dsseMul(mCol3.get256(), dsseShuffle(vec.get256(), vec.get256(), _MM_SHUFFLE(3, 3, 3, 3))))));
}

inline const Vector4d Matrix4d::operator * (const Vector3d & vec) const
{
	return Vector4d(
		dsseAdd(
			dsseAdd(dsseMul(mCol0.get256(), dsseShuffle(vec.get256(), vec.get256(), _MM_SHUFFLE(0, 0, 0, 0))), dsseMul(mCol1.get256(), dsseShuffle(vec.get256(), vec.get256(), _MM_SHUFFLE(1, 1, 1, 1)))),
			dsseMul(mCol2.get256(), dsseShuffle(vec.get256(), vec.get256(), _MM_SHUFFLE(2, 2, 2, 2)))));
}

inline const Vector4d Matrix4d::operator * (const Point3 & pnt) const
{
	SSEFloat f;
	f.m128 = pnt.get128();
	const DSSEVec4 fVec = dsseFromFVec4(f);
	return Vector4d(
		dsseAdd(
			dsseAdd(dsseMul(mCol0.get256(), dsseShuffle(fVec, fVec, _MM_SHUFFLE(0, 0, 0, 0))), dsseMul(mCol1.get256(), dsseShuffle(fVec, fVec, _MM_SHUFFLE(1, 1, 1, 1)))),
			dsseAdd(dsseMul(mCol2.get256(), dsseShuffle(fVec, fVec, _MM_SHUFFLE(2, 2, 2, 2))), mCol3.get256())));
}

inline const Matrix4d Matrix4d::operator * (const Matrix4d & mat) const
{
	return Matrix4d((*this * mat.mCol0),
				   (*this * mat.mCol1),
				   (*this * mat.mCol2),
				   (*this * mat.mCol3));
}

inline Matrix4d & Matrix4d::operator *= (const Matrix4d & mat)
{
	*this = *this * mat;
	return *this;
}

inline const Matrix4d Matrix4d::operator * (const Transform3 & tfrm) const
{
	SSEFloat c0, c1, c2;
	c0.m128 = tfrm.getCol0().get128();
	c1.m128 = tfrm.getCol1().get128();
	c2.m128 = tfrm.getCol2().get128();
	return Matrix4d(
		(*this * Vector3d(dsseFromFVec4(c0))),
		(*this * Vector3d(dsseFromFVec4(c1))),
		(*this * Vector3d(dsseFromFVec4(c2))),
		(*this * Point3(tfrm.getCol3()))
	);
}

inline Matrix4d & Matrix4d::operator *= (const Transform3 & tfrm)
{
	*this = *this * tfrm;
	return *this;
}

inline const Matrix4d mulPerElem(const Matrix4d & mat0, const Matrix4d & mat1)
{
	return Matrix4d(mulPerElem(mat0.getCol0(), mat1.getCol0()),
				   mulPerElem(mat0.getCol1(), mat1.getCol1()),
				   mulPerElem(mat0.getCol2(), mat1.getCol2()),
				   mulPerElem(mat0.getCol3(), mat1.getCol3()));
}

inline const Matrix4d Matrix4d::identity()
{
	return Matrix4d(Vector4d::xAxis(),
				   Vector4d::yAxis(),
				   Vector4d::zAxis(),
				   Vector4d::wAxis());
}

inline Matrix4d & Matrix4d::setUpper3x3(const Matrix3d & mat3)
{
	mCol0.setXYZ(mat3.getCol0());
	mCol1.setXYZ(mat3.getCol1());
	mCol2.setXYZ(mat3.getCol2());
	return *this;
}

inline const Matrix3d Matrix4d::getUpper3x3() const
{
	return Matrix3d(mCol0.getXYZ(), mCol1.getXYZ(), mCol2.getXYZ());
}

inline Matrix4d & Matrix4d::setTranslation(const Vector3d & translateVec)
{
	mCol3.setXYZ(translateVec);
	return *this;
}

inline const Vector3d Matrix4d::getTranslation() const
{
	return mCol3.getXYZ();
}

inline const Matrix4d Matrix4d::rotationX(double radians)
{
	return rotationX(DoubleInVec(radians));
}

inline const Matrix4d Matrix4d::rotationX(const DoubleInVec & radians)
{
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_z[4]) = { 0, 0, 0xFFFFFFFFFFFFFFFF, 0 };
	const DSSEVec4 zero = dsseSetZero();
	DSSEVec4 s, c, res1, res2;
	dsseSinfCosf(radians.get256(), &s, &c);
	res1 = dsseSelect(zero, c, select_y);
	res1 = dsseSelect(res1, s, select_z);
	res2 = dsseSelect(zero, dsseNegatef(s), select_y);
	res2 = dsseSelect(res2, c, select_z);
	return Matrix4d(Vector4d::xAxis(),
				   Vector4d(res1),
				   Vector4d(res2),
				   Vector4d::wAxis());
}

inline const Matrix4d Matrix4d::rotationY(double radians)
{
	return rotationY(DoubleInVec(radians));
}

inline const Matrix4d Matrix4d::rotationY(const DoubleInVec & radians)
{
	VECTORMATH_ALIGNED(unsigned long long select_x[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_z[4]) = { 0, 0, 0xFFFFFFFFFFFFFFFF, 0 };
	const DSSEVec4 zero = dsseSetZero();
	DSSEVec4 s, c, res0, res2;
	dsseSinfCosf(radians.get256(), &s, &c);
	res0 = dsseSelect(zero, c, select_x);
	res0 = dsseSelect(res0, dsseNegatef(s), select_z);
	res2 = dsseSelect(zero, s, select_x);
	res2 = dsseSelect(res2, c, select_z);
	return Matrix4d(Vector4d(res0),
				   Vector4d::yAxis(),
				   Vector4d(res2),
				   Vector4d::wAxis());
}

inline const Matrix4d Matrix4d::rotationZ(double radians)
{
	return rotationZ(DoubleInVec(radians));
}

inline const Matrix4d Matrix4d::rotationZ(const DoubleInVec & radians)
{
	VECTORMATH_ALIGNED(unsigned long long select_x[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	const DSSEVec4 zero = dsseSetZero();
	
	DSSEVec4 s, c, res0, res1;
	dsseSinfCosf(radians.get256(), &s, &c);
	res0 = dsseSelect(zero, c, select_x);
	res0 = dsseSelect(res0, s, select_y);
	res1 = dsseSelect(zero, dsseNegatef(s), select_x);
	res1 = dsseSelect(res1, c, select_y);
	return Matrix4d(Vector4d(res0),
				   Vector4d(res1),
				   Vector4d::zAxis(),
				   Vector4d::wAxis());
}

inline const Matrix4d Matrix4d::rotationZYX(const Vector3d & radiansXYZ)
{
	DSSEVec4 angles, s, negS, c, X0, X1, Y0, Y1, Z0, Z1, tmp;
	angles = Vector4d(radiansXYZ, 0.0f).get256();
	dsseSinfCosf(angles, &s, &c);
	negS = dsseNegatef(s);
	Z0 = dsseMergeL(c, s);
	Z1 = dsseMergeL(negS, c);
	VECTORMATH_ALIGNED(unsigned long long select_xyz[4]) = { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0 };
	Z1 = dsseAnd(Z1, dsseLoadu((double *)select_xyz));
	Y0 = dsseShuffle(c, negS, _MM_SHUFFLE(0, 1, 1, 1));
	Y1 = dsseShuffle(s, c, _MM_SHUFFLE(0, 1, 1, 1));
	X0 = dsseSplat(s, 0);
	X1 = dsseSplat(c, 0);
	tmp = dsseMul(Z0, Y1);
	return Matrix4d(Vector4d(dsseMul(Z0, Y0)),
				   Vector4d(dsseMAdd(Z1, X1, dsseMul(tmp, X0))),
				   Vector4d(dsseMSub(Z1, X0, dsseMul(tmp, X1))),
				   Vector4d::wAxis());
}

inline const Matrix4d Matrix4d::rotation(double radians, const Vector3d & unitVec)
{
	return rotation(DoubleInVec(radians), unitVec);
}

inline const Matrix4d Matrix4d::rotation(const DoubleInVec & radians, const Vector3d & unitVec)
{
	DSSEVec4 axis, s, c, oneMinusC, axisS, negAxisS, xxxx, yyyy, zzzz, tmp0, tmp1, tmp2;
	axis = unitVec.get256();
	dsseSinfCosf(radians.get256(), &s, &c);
	xxxx = dsseSplat(axis, 0);
	yyyy = dsseSplat(axis, 1);
	zzzz = dsseSplat(axis, 2);
	oneMinusC = dsseSub(dsseSet1(1.0), c);
	axisS = dsseMul(axis, s);
	negAxisS = dsseNegatef(axisS);
	VECTORMATH_ALIGNED(unsigned long long select_x[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_z[4]) = { 0, 0, 0xFFFFFFFFFFFFFFFF, 0 };
	tmp0 = dsseShuffle(axisS, axisS, _MM_SHUFFLE(0, 0, 2, 0));
	tmp0 = dsseSelect(tmp0, dsseSplat(negAxisS, 1), select_z);
	tmp1 = dsseSelect(dsseSplat(axisS, 0), dsseSplat(negAxisS, 2), select_x);
	tmp2 = dsseShuffle(axisS, axisS, _MM_SHUFFLE(0, 0, 0, 1));
	tmp2 = dsseSelect(tmp2, dsseSplat(negAxisS, 0), select_y);
	tmp0 = dsseSelect(tmp0, c, select_x);
	tmp1 = dsseSelect(tmp1, c, select_y);
	tmp2 = dsseSelect(tmp2, c, select_z);
	VECTORMATH_ALIGNED(unsigned long long select_xyz[4]) = { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0 };
	axis = dsseAnd(axis, dsseLoadu((double *)select_xyz));
	tmp0 = dsseAnd(tmp0, dsseLoadu((double *)select_xyz));
	tmp1 = dsseAnd(tmp1, dsseLoadu((double *)select_xyz));
	tmp2 = dsseAnd(tmp2, dsseLoadu((double *)select_xyz));
	return Matrix4d(Vector4d(dsseMAdd(dsseMul(axis, xxxx), oneMinusC, tmp0)),
				   Vector4d(dsseMAdd(dsseMul(axis, yyyy), oneMinusC, tmp1)),
				   Vector4d(dsseMAdd(dsseMul(axis, zzzz), oneMinusC, tmp2)),
				   Vector4d::wAxis());
}

inline const Matrix4d Matrix4d::rotation(const Quat & unitQuat)
{
	return Matrix4d(Transform3::rotation(unitQuat));
}

inline const Matrix4d Matrix4d::scale(const Vector3d & scaleVec)
{
	DSSEVec4 zero = dsseSetZero();
	VECTORMATH_ALIGNED(unsigned long long select_x[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_z[4]) = { 0, 0, 0xFFFFFFFFFFFFFFFF, 0 };
	return Matrix4d(Vector4d(dsseSelect(zero, scaleVec.get256(), select_x)),
				   Vector4d(dsseSelect(zero, scaleVec.get256(), select_y)),
				   Vector4d(dsseSelect(zero, scaleVec.get256(), select_z)),
				   Vector4d::wAxis());
}

inline const Matrix4d appendScale(const Matrix4d & mat, const Vector3d & scaleVec)
{
	return Matrix4d((mat.getCol0() * scaleVec.getX()),
				   (mat.getCol1() * scaleVec.getY()),
				   (mat.getCol2() * scaleVec.getZ()),
				   mat.getCol3());
}

inline const Matrix4d prependScale(const Vector3d & scaleVec, const Matrix4d & mat)
{
	const Vector4d scale4 = Vector4d(scaleVec, 1.0);
	return Matrix4d(mulPerElem(mat.getCol0(), scale4),
				   mulPerElem(mat.getCol1(), scale4),
				   mulPerElem(mat.getCol2(), scale4),
				   mulPerElem(mat.getCol3(), scale4));
}

inline const Matrix4d Matrix4d::translation(const Vector3d & translateVec)
{
	return Matrix4d(Vector4d::xAxis(),
				   Vector4d::yAxis(),
				   Vector4d::zAxis(),
				   Vector4d(translateVec, 1.0));
}

inline const Matrix4d Matrix4d::lookAtLH(const Point3 & eyePos, const Point3 & lookAtPos, const Vector3d & upVec)
{
	Matrix4d m4EyeFrame;
	Vector3d v3X, v3Y, v3Z;
	v3Y = normalize(upVec);
	v3Z = normalize(Vector3d(lookAtPos) - Vector3d(eyePos));
	v3X = normalize(cross(v3Y, v3Z));
	v3Y = cross(v3Z, v3X);
	m4EyeFrame = Matrix4d(Vector4d(v3X), Vector4d(v3Y), Vector4d(v3Z), Vector4d(eyePos));
	return orthoInverse(m4EyeFrame);
}

inline const Matrix4d Matrix4d::lookAtRH(const Point3 & eyePos, const Point3 & lookAtPos, const Vector3d & upVec)
{
	Matrix4d m4EyeFrame;
	Vector3d v3X, v3Y, v3Z;
	v3Y = normalize(upVec);
	v3Z = normalize(Vector3d(eyePos) - Vector3d(lookAtPos));
	v3X = normalize(cross(v3Y, v3Z));
	v3Y = cross(v3Z, v3X);
	m4EyeFrame = Matrix4d(Vector4d(v3X), Vector4d(v3Y), Vector4d(v3Z), Vector4d(eyePos));
	return orthoInverse(m4EyeFrame);
}

inline const Matrix4d Matrix4d::frustumLH(double left, double right, double bottom, double top, double zNear, double zFar)
{
	DSSEVec4 lbf, rtn;
	DSSEVec4 diff, inv_diff;
	DSSEVec4 diagonalXY, diagonalZ, column, near1, near2;
	DSSEVec4 zero = dsseSetZero();
	DSSEVec4 l, f, r, n, b, t;
	l.d[0] = left;
	f.d[0] = zFar;
	r.d[0] = right;
	n.d[0] = zNear;
	b.d[0] = bottom;
	t.d[0] = top;
	lbf = dsseMergeH(l, f);
	rtn = dsseMergeH(r, n);
	lbf = dsseMergeH(lbf, b);
	rtn = dsseMergeH(rtn, t);
	diff = dsseSub(rtn, lbf);
	inv_diff = dsseRecipf(diff);
	near1 = dsseSplat(n, 0);
	near2 = dsseAdd(near1, near1);
	diagonalXY = dsseMul(near2, inv_diff);
	diagonalZ = dsseMul(near1, inv_diff);
	column = dsseMul(lbf, inv_diff);
	VECTORMATH_ALIGNED(unsigned long long select_x[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_z[4]) = { 0, 0, 0xFFFFFFFFFFFFFFFF, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_xyw[4]) = { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0, 0xFFFFFFFFFFFFFFFF };
	Matrix4d m = Matrix4d(Vector4d(dsseSelect(zero, diagonalXY, select_x)),
		Vector4d(dsseSelect(zero, diagonalXY, select_y)),
		Vector4d(dsseSelect(column, dsseSetr(0.0f, 0.0f, INFINITY, -1.0), select_xyw)),
		Vector4d(dsseSelect(zero, dsseMul(diagonalZ, dsseSplat(f, 0)), select_z)));
	m.setCol2(-m.getCol2()); // so far we've constructed RH matrix, make it LH
	return m;
}

//========================================= #TheForgeMathExtensionsBegin ================================================
#define CONSTRUCT_TRANSPOSED_MAT4d(m11,m12,m13,m14,m21,m22,m23,m24,m31,m32,m33,m34,m41,m42,m43,m44) \
	Matrix4d(Vector4d(m11,m21,m31,m41),\
		 Vector4d(m12,m22,m32,m42),\
		 Vector4d(m13,m23,m33,m43),\
		 Vector4d(m14,m24,m34,m44))

//----------------------------------------------------------------------------------------

//#define USE_VERTICAL_FIELD_OF_VIEW 0	// The Forge uses perspective() with horizontal field of view, defined below

inline double FovHorizontalToVertical(double fovX, double aspectInverse)
{
	return 2.0 * atan(tan(fovX / 2.0) * aspectInverse);
}

inline double FovVerticalToHorizontal(double fovY, double aspectInverse)
{
	return 2.0 * atan(tan(fovY / 2.0) / aspectInverse);
}

inline const Matrix4d Matrix4d::perspectiveLH(double fovRadians, double aspectInverse, double zNear, double zFar)
{
	static const double VECTORMATH_PI_OVER_2 = 1.570796327;

	double f, rangeInv;
	DSSEVec4 tmp;
	DSSEVec4 col0, col1, col2, col3;

#if USE_VERTICAL_FIELD_OF_VIEW
	fovRadians = FovVerticalToHorizontal(fovRadians, aspectInverse);
#endif

#if defined(__linux__)
	// linux build uses c++11 standard
	f = std::tan(VECTORMATH_PI_OVER_2 - fovRadians * 0.5);
#else
	f = ::tan(VECTORMATH_PI_OVER_2 - fovRadians * 0.5);
#endif

	rangeInv = 1.0 / (zFar - zNear);

	const DSSEVec4 zero = dsseSetZero();
	tmp = zero;
	tmp.d[0] = f;
	col0 = tmp;
	tmp = zero;
	tmp.d[1] = f / aspectInverse;
	col1 = tmp;
	tmp = zero;
	tmp.d[2] = (zFar)* rangeInv;
	tmp.d[3] = +1.0;
	col2 = tmp;
	tmp = zero;
	tmp.d[2] = -zNear * zFar * rangeInv;
	col3 = tmp;

	return Matrix4d(Vector4d(col0), Vector4d(col1), Vector4d(col2), Vector4d(col3));
}

inline const Matrix4d Matrix4d::perspectiveRH(double fovRadians, double aspectInverse, double zNear, double zFar)
{
	Matrix4d m = perspectiveLH(fovRadians, aspectInverse, zNear, zFar);
	m.setCol2(-m.getCol2());
	return m;
}

inline const Matrix4d Matrix4d::perspectiveLH_ReverseZ(double fovRadians, double aspectInverse, double zNear, double zFar)
{
	Matrix4d perspMatrix = perspectiveLH(fovRadians, aspectInverse, zNear, zFar);

	const Vector4d &col2 = perspMatrix.mCol2;
	const Vector4d &col3 = perspMatrix.mCol3;
	perspMatrix.mCol2.setZ(col2.getW() - col2.getZ());
	perspMatrix.mCol3.setZ(-col3.getZ());

	return perspMatrix;
}

inline const Matrix4d Matrix4d::orthographicLH(double left, double right, double bottom, double top, double zNear, double zFar)
{
	DSSEVec4 lbn, rtf;
	DSSEVec4 diff, sum, inv_diff, neg_inv_diff;
	DSSEVec4 diagonal, column;
	DSSEVec4 zero = dsseSetZero();
	DSSEVec4 l, f, r, n, b, t;
	l = dsseSet1(left);
	f = dsseSet1(zFar);
	r = dsseSet1(right);
	n = dsseSet1(zNear);
	b = dsseSet1(bottom);
	t = dsseSet1(top);
	lbn = dsseMergeH(l, n);
	rtf = dsseMergeH(r, f);
	lbn = dsseMergeH(lbn, b);
	rtf = dsseMergeH(rtf, t);
	diff = dsseSub(rtf, lbn);
	inv_diff = dsseRecipf(diff);
	neg_inv_diff = dsseNegatef(inv_diff);
	VECTORMATH_ALIGNED(unsigned long long select_x[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_z[4]) = { 0, 0, 0xFFFFFFFFFFFFFFFF, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_w[4]) = { 0, 0, 0, 0xFFFFFFFFFFFFFFFF };
	sum = dsseAdd(rtf, dsseSelect(lbn, dsseSub(n, f), select_z));
	diagonal = dsseAdd(inv_diff, dsseSelect(inv_diff, zero, select_z));
	column = dsseMul(sum, neg_inv_diff);
	return Matrix4d(Vector4d(dsseSelect(zero, diagonal, select_x)),
		Vector4d(dsseSelect(zero, diagonal, select_y)),
		Vector4d(dsseSelect(zero, diagonal, select_z)),
		Vector4d(dsseSelect(column, dsseSet1(1.0), select_w)));
}

inline const Matrix4d Matrix4d::orthographicRH(double left, double right, double bottom, double top, double zNear, double zFar)
{
	Matrix4d m = orthographicLH(left, right, bottom, top, zNear, zFar);
	m.setCol2(-m.getCol2());
	return m;
}

inline const Matrix4d Matrix4d::orthographicLH_ReverseZ(double left, double right, double bottom, double top, double zNear, double zFar)
{
	Matrix4d orthoMatrix = orthographicLH(left, right, bottom, top, zNear, zFar);

	const Vector4d &col2 = orthoMatrix.mCol2;
	const Vector4d &col3 = orthoMatrix.mCol3;
	orthoMatrix.mCol2.setZ(-col2.getZ());
	orthoMatrix.mCol3.setZ(-col3.getZ() * zFar / zNear);

	return orthoMatrix;
}

inline const Matrix4d Matrix4d::cubeProjectionLH(const double zNear, const double zFar)
{
	return CONSTRUCT_TRANSPOSED_MAT4d(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, zFar / (zFar - zNear), (zFar * zNear) / (zNear - zFar),
		0, 0, 1, 0);
}
inline const Matrix4d Matrix4d::cubeProjectionRH(const double zNear, const double zFar)
{
	Matrix4d m = cubeProjectionLH(zNear, zFar);
	m.setCol2(-m.getCol2());
	return m;
}

inline const Matrix4d Matrix4d::cubeView(const unsigned int side)
{
	switch (side)
	{
	case POSITIVE_X:
		return CONSTRUCT_TRANSPOSED_MAT4d(
			0, 0, -1, 0,
			0, 1, 0, 0,
			1, 0, 0, 0,
			0, 0, 0, 1);
	case NEGATIVE_X:
		return CONSTRUCT_TRANSPOSED_MAT4d(
			0, 0, 1, 0,
			0, 1, 0, 0,
			-1, 0, 0, 0,
			0, 0, 0, 1);
	case POSITIVE_Y:
		return CONSTRUCT_TRANSPOSED_MAT4d(
			1, 0, 0, 0,
			0, 0, -1, 0,
			0, 1, 0, 0,
			0, 0, 0, 1);
	case NEGATIVE_Y:
		return CONSTRUCT_TRANSPOSED_MAT4d(
			1, 0, 0, 0,
			0, 0, 1, 0,
			0, -1, 0, 0,
			0, 0, 0, 1);
	case POSITIVE_Z:
		return CONSTRUCT_TRANSPOSED_MAT4d(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);
		//case NEGATIVE_Z:
	default:
		return CONSTRUCT_TRANSPOSED_MAT4d(
			-1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, -1, 0,
			0, 0, 0, 1);
	}
}

inline void Matrix4d::extractFrustumClipPlanes(const Matrix4d& vp, Vector4d& rcp, Vector4d& lcp, Vector4d& tcp, Vector4d& bcp, Vector4d& fcp, Vector4d& ncp, bool const normalizePlanes)
{
	// Left plane
	lcp = vp.getRow(3) + vp.getRow(0);

	// Right plane
	rcp = vp.getRow(3) - vp.getRow(0);

	// Bottom plane
	bcp = vp.getRow(3) + vp.getRow(1);

	// Top plane
	tcp = vp.getRow(3) - vp.getRow(1);

	// Near plane
	ncp = vp.getRow(3) + vp.getRow(2);

	// Far plane
	fcp = vp.getRow(3) - vp.getRow(2);

	// Normalize if needed
	if (normalizePlanes)
	{
		double lcp_norm = length(lcp.getXYZ());
		lcp /= lcp_norm;

		double rcp_norm = length(rcp.getXYZ());
		rcp /= rcp_norm;

		double bcp_norm = length(bcp.getXYZ());
		bcp /= bcp_norm;

		double tcp_norm = length(tcp.getXYZ());
		tcp /= tcp_norm;

		double ncp_norm = length(ncp.getXYZ());
		ncp /= ncp_norm;

		double fcp_norm = length(fcp.getXYZ());
		fcp /= fcp_norm;
	}
}

inline const Matrix4d Matrix4d::rotationYX(const double radiansY, const double radiansX)
{
	// Note that:
	//  rotateYX(-y,-x)*rotateXY(x,y) == mat4::identity()
	// which means that
	//  inverse(rotateXY(x,y)) = rotateYX(-y,-x)

	//return mat4::rotationY(angleY) * mat4::rotationX(angleX);

	//[	1,		 0,		 0,		 0]
	//[	0,		 c,		-s,		 0]
	//[	0,		 s,		 c,		 0]
	//[	0,		 0,		 0,		 1]
	//
	//[ o, 0, i, 0]	[   o,		is,		ic,		0]
	//[ 0, 1, 0, 0]	[   0,		c ,		-s,		0]
	//[-i, 0, o, 0]	[  -i,		os,		oc,		0]
	//[ 0, 0, 0, 1]	[   0,		0 ,		0 ,		1]

	const double cosX = cos(radiansX), sinX = sin(radiansX);
	const double cosY = cos(radiansY), sinY = sin(radiansY);
	return CONSTRUCT_TRANSPOSED_MAT4d(
		cosY, sinY*sinX, sinY*cosX, 0,
		0, cosX, -sinX, 0,
		-sinY, cosY*sinX, cosY*cosX, 0,
		0, 0, 0, 1
	);
}

inline const Matrix4d Matrix4d::rotationXY(const double radiansX, const double radiansY)
{	//same as: return mat4::rotationX(angleX) * mat4::rotationY(angleY);
	const double cosX = cos(radiansX), sinX = sin(radiansX);
	const double cosY = cos(radiansY), sinY = sin(radiansY);

	return CONSTRUCT_TRANSPOSED_MAT4d(
		cosY, 0, sinY, 0,
		sinX * sinY, cosX, -sinX * cosY, 0,
		cosX *-sinY, sinX, cosX * cosY, 0,
		0, 0, 0, 1);
}
//========================================= #TheForgeMathExtensionsEnd ==================================================

inline const Matrix4d select(const Matrix4d & mat0, const Matrix4d & mat1, bool select1)
{
	return Matrix4d(select(mat0.getCol0(), mat1.getCol0(), select1),
		select(mat0.getCol1(), mat1.getCol1(), select1),
		select(mat0.getCol2(), mat1.getCol2(), select1),
		select(mat0.getCol3(), mat1.getCol3(), select1));
}

inline const Matrix4d select(const Matrix4d & mat0, const Matrix4d & mat1, const BoolInVec & select1)
{
	return Matrix4d(select(mat0.getCol0(), mat1.getCol0(), select1),
		select(mat0.getCol1(), mat1.getCol1(), select1),
		select(mat0.getCol2(), mat1.getCol2(), select1),
		select(mat0.getCol3(), mat1.getCol3(), select1));
}

#ifdef VECTORMATH_DEBUG

inline void print(const Matrix4d & mat)
{
	print(mat.getRow(0));
	print(mat.getRow(1));
	print(mat.getRow(2));
	print(mat.getRow(3));
}

inline void print(const Matrix4d & mat, const char * name)
{
	std::printf("%s:\n", name);
	print(mat);
}

#endif // VECTORMATH_DEBUG

// ========================================================
// Transform3
// ========================================================

inline Transform3::Transform3(const Transform3 & tfrm)
{
    mCol0 = tfrm.mCol0;
    mCol1 = tfrm.mCol1;
    mCol2 = tfrm.mCol2;
    mCol3 = tfrm.mCol3;
}

inline Transform3::Transform3(float scalar)
{
    mCol0 = Vector3(scalar);
    mCol1 = Vector3(scalar);
    mCol2 = Vector3(scalar);
    mCol3 = Vector3(scalar);
}

inline Transform3::Transform3(const FloatInVec & scalar)
{
    mCol0 = Vector3(scalar);
    mCol1 = Vector3(scalar);
    mCol2 = Vector3(scalar);
    mCol3 = Vector3(scalar);
}

inline Transform3::Transform3(const Vector3 & _col0, const Vector3 & _col1, const Vector3 & _col2, const Vector3 & _col3)
{
    mCol0 = _col0;
    mCol1 = _col1;
    mCol2 = _col2;
    mCol3 = _col3;
}

inline Transform3::Transform3(const Matrix3 & tfrm, const Vector3 & translateVec)
{
    this->setUpper3x3(tfrm);
    this->setTranslation(translateVec);
}

inline Transform3::Transform3(const Quat & unitQuat, const Vector3 & translateVec)
{
    this->setUpper3x3(Matrix3(unitQuat));
    this->setTranslation(translateVec);
}

inline Transform3 & Transform3::setCol0(const Vector3 & _col0)
{
    mCol0 = _col0;
    return *this;
}

inline Transform3 & Transform3::setCol1(const Vector3 & _col1)
{
    mCol1 = _col1;
    return *this;
}

inline Transform3 & Transform3::setCol2(const Vector3 & _col2)
{
    mCol2 = _col2;
    return *this;
}

inline Transform3 & Transform3::setCol3(const Vector3 & _col3)
{
    mCol3 = _col3;
    return *this;
}

inline Transform3 & Transform3::setCol(int col, const Vector3 & vec)
{
    *(&mCol0 + col) = vec;
    return *this;
}

inline Transform3 & Transform3::setRow(int row, const Vector4 & vec)
{
    mCol0.setElem(row, vec.getElem(0));
    mCol1.setElem(row, vec.getElem(1));
    mCol2.setElem(row, vec.getElem(2));
    mCol3.setElem(row, vec.getElem(3));
    return *this;
}

inline Transform3 & Transform3::setElem(int col, int row, float val)
{
    (*this)[col].setElem(row, val);
    return *this;
}

inline Transform3 & Transform3::setElem(int col, int row, const FloatInVec & val)
{
    Vector3 tmpV3_0;
    tmpV3_0 = this->getCol(col);
    tmpV3_0.setElem(row, val);
    this->setCol(col, tmpV3_0);
    return *this;
}

inline const FloatInVec Transform3::getElem(int col, int row) const
{
    return this->getCol(col).getElem(row);
}

inline const Vector3 Transform3::getCol0() const
{
    return mCol0;
}

inline const Vector3 Transform3::getCol1() const
{
    return mCol1;
}

inline const Vector3 Transform3::getCol2() const
{
    return mCol2;
}

inline const Vector3 Transform3::getCol3() const
{
    return mCol3;
}

inline const Vector3 Transform3::getCol(int col) const
{
    return *(&mCol0 + col);
}

inline const Vector4 Transform3::getRow(int row) const
{
    return Vector4(mCol0.getElem(row), mCol1.getElem(row), mCol2.getElem(row), mCol3.getElem(row));
}

inline Vector3 & Transform3::operator[](int col)
{
    return *(&mCol0 + col);
}

inline const Vector3 Transform3::operator[](int col) const
{
    return *(&mCol0 + col);
}

inline Transform3 & Transform3::operator = (const Transform3 & tfrm)
{
    mCol0 = tfrm.mCol0;
    mCol1 = tfrm.mCol1;
    mCol2 = tfrm.mCol2;
    mCol3 = tfrm.mCol3;
    return *this;
}

inline const Transform3 inverse(const Transform3 & tfrm)
{
    __m128 inv0, inv1, inv2, inv3;
    __m128 tmp0, tmp1, tmp2, tmp3, tmp4, dot, invdet;
    __m128 xxxx, yyyy, zzzz;
    tmp2 = sseVecCross(tfrm.getCol0().get128(), tfrm.getCol1().get128());
    tmp0 = sseVecCross(tfrm.getCol1().get128(), tfrm.getCol2().get128());
    tmp1 = sseVecCross(tfrm.getCol2().get128(), tfrm.getCol0().get128());
    inv3 = sseNegatef(tfrm.getCol3().get128());
    dot = sseVecDot3(tmp2, tfrm.getCol2().get128());
    dot = sseSplat(dot, 0);
    invdet = sseRecipf(dot);
    tmp3 = sseMergeH(tmp0, tmp2);
    tmp4 = sseMergeL(tmp0, tmp2);
    inv0 = sseMergeH(tmp3, tmp1);
    xxxx = sseSplat(inv3, 0);
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    inv1 = _mm_shuffle_ps(tmp3, tmp3, _MM_SHUFFLE(0, 3, 2, 2));
    inv1 = sseSelect(inv1, tmp1, select_y);
    inv2 = _mm_shuffle_ps(tmp4, tmp4, _MM_SHUFFLE(0, 1, 1, 0));
    inv2 = sseSelect(inv2, sseSplat(tmp1, 2), select_y);
    yyyy = sseSplat(inv3, 1);
    zzzz = sseSplat(inv3, 2);
    inv3 = _mm_mul_ps(inv0, xxxx);
    inv3 = sseMAdd(inv1, yyyy, inv3);
    inv3 = sseMAdd(inv2, zzzz, inv3);
    inv0 = _mm_mul_ps(inv0, invdet);
    inv1 = _mm_mul_ps(inv1, invdet);
    inv2 = _mm_mul_ps(inv2, invdet);
    inv3 = _mm_mul_ps(inv3, invdet);
    return Transform3(Vector3(inv0), Vector3(inv1), Vector3(inv2), Vector3(inv3));
}

inline const Transform3 orthoInverse(const Transform3 & tfrm)
{
    __m128 inv0, inv1, inv2, inv3;
    __m128 tmp0, tmp1;
    __m128 xxxx, yyyy, zzzz;
    tmp0 = sseMergeH(tfrm.getCol0().get128(), tfrm.getCol2().get128());
    tmp1 = sseMergeL(tfrm.getCol0().get128(), tfrm.getCol2().get128());
    inv3 = sseNegatef(tfrm.getCol3().get128());
    inv0 = sseMergeH(tmp0, tfrm.getCol1().get128());
    xxxx = sseSplat(inv3, 0);
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    inv1 = _mm_shuffle_ps(tmp0, tmp0, _MM_SHUFFLE(0, 3, 2, 2));
    inv1 = sseSelect(inv1, tfrm.getCol1().get128(), select_y);
    inv2 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(0, 1, 1, 0));
    inv2 = sseSelect(inv2, sseSplat(tfrm.getCol1().get128(), 2), select_y);
    yyyy = sseSplat(inv3, 1);
    zzzz = sseSplat(inv3, 2);
    inv3 = _mm_mul_ps(inv0, xxxx);
    inv3 = sseMAdd(inv1, yyyy, inv3);
    inv3 = sseMAdd(inv2, zzzz, inv3);
    return Transform3(Vector3(inv0), Vector3(inv1), Vector3(inv2), Vector3(inv3));
}

inline const Transform3 absPerElem(const Transform3 & tfrm)
{
    return Transform3(absPerElem(tfrm.getCol0()),
                      absPerElem(tfrm.getCol1()),
                      absPerElem(tfrm.getCol2()),
                      absPerElem(tfrm.getCol3()));
}

inline const Vector3 Transform3::operator * (const Vector3 & vec) const
{
    __m128 res;
    __m128 xxxx, yyyy, zzzz;
    xxxx = sseSplat(vec.get128(), 0);
    yyyy = sseSplat(vec.get128(), 1);
    zzzz = sseSplat(vec.get128(), 2);
    res = _mm_mul_ps(mCol0.get128(), xxxx);
    res = sseMAdd(mCol1.get128(), yyyy, res);
    res = sseMAdd(mCol2.get128(), zzzz, res);
    return Vector3(res);
}

inline const Point3 Transform3::operator * (const Point3 & pnt) const
{
    __m128 tmp0, tmp1, res;
    __m128 xxxx, yyyy, zzzz;
    xxxx = sseSplat(pnt.get128(), 0);
    yyyy = sseSplat(pnt.get128(), 1);
    zzzz = sseSplat(pnt.get128(), 2);
    tmp0 = _mm_mul_ps(mCol0.get128(), xxxx);
    tmp1 = _mm_mul_ps(mCol1.get128(), yyyy);
    tmp0 = sseMAdd(mCol2.get128(), zzzz, tmp0);
    tmp1 = _mm_add_ps(mCol3.get128(), tmp1);
    res = _mm_add_ps(tmp0, tmp1);
    return Point3(res);
}

inline const Transform3 Transform3::operator * (const Transform3 & tfrm) const
{
    return Transform3((*this * tfrm.mCol0),
                      (*this * tfrm.mCol1),
                      (*this * tfrm.mCol2),
                      Vector3((*this * Point3(tfrm.mCol3))));
}

inline Transform3 & Transform3::operator *= (const Transform3 & tfrm)
{
    *this = *this * tfrm;
    return *this;
}

inline const Transform3 mulPerElem(const Transform3 & tfrm0, const Transform3 & tfrm1)
{
    return Transform3(mulPerElem(tfrm0.getCol0(), tfrm1.getCol0()),
                      mulPerElem(tfrm0.getCol1(), tfrm1.getCol1()),
                      mulPerElem(tfrm0.getCol2(), tfrm1.getCol2()),
                      mulPerElem(tfrm0.getCol3(), tfrm1.getCol3()));
}

inline const Transform3 Transform3::identity()
{
    return Transform3(Vector3::xAxis(),
                      Vector3::yAxis(),
                      Vector3::zAxis(),
                      Vector3(0.0f));
}

inline Transform3 & Transform3::setUpper3x3(const Matrix3 & tfrm)
{
    mCol0 = tfrm.getCol0();
    mCol1 = tfrm.getCol1();
    mCol2 = tfrm.getCol2();
    return *this;
}

inline const Matrix3 Transform3::getUpper3x3() const
{
    return Matrix3(mCol0, mCol1, mCol2);
}

inline Transform3 & Transform3::setTranslation(const Vector3 & translateVec)
{
    mCol3 = translateVec;
    return *this;
}

inline const Vector3 Transform3::getTranslation() const
{
    return mCol3;
}

inline const Transform3 Transform3::rotationX(float radians)
{
    return rotationX(FloatInVec(radians));
}

inline const Transform3 Transform3::rotationX(const FloatInVec & radians)
{
    __m128 s, c, res1, res2;
    __m128 zero;
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    zero = _mm_setzero_ps();
    sseSinfCosf(radians.get128(), &s, &c);
    res1 = sseSelect(zero, c, select_y);
    res1 = sseSelect(res1, s, select_z);
    res2 = sseSelect(zero, sseNegatef(s), select_y);
    res2 = sseSelect(res2, c, select_z);
    return Transform3(Vector3::xAxis(),
                      Vector3(res1),
                      Vector3(res2),
                      Vector3(_mm_setzero_ps()));
}

inline const Transform3 Transform3::rotationY(float radians)
{
    return rotationY(FloatInVec(radians));
}

inline const Transform3 Transform3::rotationY(const FloatInVec & radians)
{
    __m128 s, c, res0, res2;
    __m128 zero;
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    zero = _mm_setzero_ps();
    sseSinfCosf(radians.get128(), &s, &c);
    res0 = sseSelect(zero, c, select_x);
    res0 = sseSelect(res0, sseNegatef(s), select_z);
    res2 = sseSelect(zero, s, select_x);
    res2 = sseSelect(res2, c, select_z);
    return Transform3(Vector3(res0),
                      Vector3::yAxis(),
                      Vector3(res2),
                      Vector3(0.0f));
}

inline const Transform3 Transform3::rotationZ(float radians)
{
    return rotationZ(FloatInVec(radians));
}

inline const Transform3 Transform3::rotationZ(const FloatInVec & radians)
{
    __m128 s, c, res0, res1;
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    __m128 zero = _mm_setzero_ps();
    sseSinfCosf(radians.get128(), &s, &c);
    res0 = sseSelect(zero, c, select_x);
    res0 = sseSelect(res0, s, select_y);
    res1 = sseSelect(zero, sseNegatef(s), select_x);
    res1 = sseSelect(res1, c, select_y);
    return Transform3(Vector3(res0),
                      Vector3(res1),
                      Vector3::zAxis(),
                      Vector3(0.0f));
}

inline const Transform3 Transform3::rotationZYX(const Vector3 & radiansXYZ)
{
    __m128 angles, s, negS, c, X0, X1, Y0, Y1, Z0, Z1, tmp;
    angles = Vector4(radiansXYZ, 0.0f).get128();
    sseSinfCosf(angles, &s, &c);
    negS = sseNegatef(s);
    Z0 = sseMergeL(c, s);
    Z1 = sseMergeL(negS, c);
    VECTORMATH_ALIGNED(unsigned int select_xyz[4]) = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0 };
    Z1 = _mm_and_ps(Z1, _mm_load_ps((float *)select_xyz));
    Y0 = _mm_shuffle_ps(c, negS, _MM_SHUFFLE(0, 1, 1, 1));
    Y1 = _mm_shuffle_ps(s, c, _MM_SHUFFLE(0, 1, 1, 1));
    X0 = sseSplat(s, 0);
    X1 = sseSplat(c, 0);
    tmp = _mm_mul_ps(Z0, Y1);
    return Transform3(Vector3(_mm_mul_ps(Z0, Y0)),
                      Vector3(sseMAdd(Z1, X1, _mm_mul_ps(tmp, X0))),
                      Vector3(sseMSub(Z1, X0, _mm_mul_ps(tmp, X1))),
                      Vector3(0.0f));
}

inline const Transform3 Transform3::rotation(float radians, const Vector3 & unitVec)
{
    return rotation(FloatInVec(radians), unitVec);
}

inline const Transform3 Transform3::rotation(const FloatInVec & radians, const Vector3 & unitVec)
{
    return Transform3(Matrix3::rotation(radians, unitVec), Vector3(0.0f));
}

inline const Transform3 Transform3::rotation(const Quat & unitQuat)
{
    return Transform3(Matrix3(unitQuat), Vector3(0.0f));
}

inline const Transform3 Transform3::scale(const Vector3 & scaleVec)
{
    __m128 zero = _mm_setzero_ps();
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    return Transform3(Vector3(sseSelect(zero, scaleVec.get128(), select_x)),
                      Vector3(sseSelect(zero, scaleVec.get128(), select_y)),
                      Vector3(sseSelect(zero, scaleVec.get128(), select_z)),
                      Vector3(0.0f));
}

inline const Transform3 appendScale(const Transform3 & tfrm, const Vector3 & scaleVec)
{
    return Transform3((tfrm.getCol0() * scaleVec.getX()),
                      (tfrm.getCol1() * scaleVec.getY()),
                      (tfrm.getCol2() * scaleVec.getZ()),
                      tfrm.getCol3());
}

inline const Transform3 prependScale(const Vector3 & scaleVec, const Transform3 & tfrm)
{
    return Transform3(mulPerElem(tfrm.getCol0(), scaleVec),
                      mulPerElem(tfrm.getCol1(), scaleVec),
                      mulPerElem(tfrm.getCol2(), scaleVec),
                      mulPerElem(tfrm.getCol3(), scaleVec));
}

inline const Transform3 Transform3::translation(const Vector3 & translateVec)
{
    return Transform3(Vector3::xAxis(),
                      Vector3::yAxis(),
                      Vector3::zAxis(),
                      translateVec);
}

inline const Transform3 select(const Transform3 & tfrm0, const Transform3 & tfrm1, bool select1)
{
    return Transform3(select(tfrm0.getCol0(), tfrm1.getCol0(), select1),
                      select(tfrm0.getCol1(), tfrm1.getCol1(), select1),
                      select(tfrm0.getCol2(), tfrm1.getCol2(), select1),
                      select(tfrm0.getCol3(), tfrm1.getCol3(), select1));
}

inline const Transform3 select(const Transform3 & tfrm0, const Transform3 & tfrm1, const BoolInVec & select1)
{
    return Transform3(select(tfrm0.getCol0(), tfrm1.getCol0(), select1),
                      select(tfrm0.getCol1(), tfrm1.getCol1(), select1),
                      select(tfrm0.getCol2(), tfrm1.getCol2(), select1),
                      select(tfrm0.getCol3(), tfrm1.getCol3(), select1));
}

#ifdef VECTORMATH_DEBUG

inline void print(const Transform3 & tfrm)
{
    print(tfrm.getRow(0));
    print(tfrm.getRow(1));
    print(tfrm.getRow(2));
}

inline void print(const Transform3 & tfrm, const char * name)
{
    std::printf("%s:\n", name);
    print(tfrm);
}

#endif // VECTORMATH_DEBUG


//CONFFX_TEST_BEGIN
//========================================= #TheForgeMathExtensionsBegin ================================================

// ========================================================
// Transform3
// ========================================================

inline const AffineTransform AffineTransform::identity()
{
	const AffineTransform ret = { Vector3(0.f), Quat::identity(), Vector3(1.f) };
	return ret;
}

#ifdef VECTORMATH_DEBUG

inline void print(const AffineTransform & tfrm)
{
	print(tfrm.translation);
	print(tfrm.rotation);
	print(tfrm.scale);
}

inline void print(const AffineTransform & tfrm, const char * name)
{
	std::printf("%s:\n", name);
	print(tfrm);
}

#endif // VECTORMATH_DEBUG
//========================================= #TheForgeMathExtensionsEnd ================================================
//CONFFX_TEST_END

// ========================================================
// Quat
// ========================================================

inline Quat::Quat(const Matrix3 & tfrm)
{
    __m128 res;
    __m128 col0, col1, col2;
    __m128 xx_yy, xx_yy_zz_xx, yy_zz_xx_yy, zz_xx_yy_zz, diagSum, diagDiff;
    __m128 zy_xz_yx, yz_zx_xy, sum, diff;
    __m128 radicand, invSqrt, scale;
    __m128 res0, res1, res2, res3;
    __m128 xx, yy, zz;

    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    VECTORMATH_ALIGNED(unsigned int select_w[4]) = { 0, 0, 0, 0xFFFFFFFF };

    col0 = tfrm.getCol0().get128();
    col1 = tfrm.getCol1().get128();
    col2 = tfrm.getCol2().get128();

    /* four cases: */
    /* trace > 0 */
    /* else */
    /*    xx largest diagonal element */
    /*    yy largest diagonal element */
    /*    zz largest diagonal element */
    /* compute quaternion for each case */

    xx_yy = sseSelect(col0, col1, select_y);
    xx_yy_zz_xx = _mm_shuffle_ps(xx_yy, xx_yy, _MM_SHUFFLE(0, 0, 1, 0));
    xx_yy_zz_xx = sseSelect(xx_yy_zz_xx, col2, select_z);
    yy_zz_xx_yy = _mm_shuffle_ps(xx_yy_zz_xx, xx_yy_zz_xx, _MM_SHUFFLE(1, 0, 2, 1));
    zz_xx_yy_zz = _mm_shuffle_ps(xx_yy_zz_xx, xx_yy_zz_xx, _MM_SHUFFLE(2, 1, 0, 2));

    diagSum  = _mm_add_ps(_mm_add_ps(xx_yy_zz_xx, yy_zz_xx_yy), zz_xx_yy_zz);
    diagDiff = _mm_sub_ps(_mm_sub_ps(xx_yy_zz_xx, yy_zz_xx_yy), zz_xx_yy_zz);
    radicand = _mm_add_ps(sseSelect(diagDiff, diagSum, select_w), _mm_set1_ps(1.0f));
    //invSqrt = sseRSqrtf(radicand);
    invSqrt = sseNewtonrapsonRSqrtf(radicand);

    zy_xz_yx = sseSelect(col0, col1, select_z);                             // zy_xz_yx = 00 01 12 03
    zy_xz_yx = _mm_shuffle_ps(zy_xz_yx, zy_xz_yx, _MM_SHUFFLE(0, 1, 2, 2)); // zy_xz_yx = 12 12 01 00
    zy_xz_yx = sseSelect(zy_xz_yx, sseSplat(col2, 0), select_y);            // zy_xz_yx = 12 20 01 00
    yz_zx_xy = sseSelect(col0, col1, select_x);                             // yz_zx_xy = 10 01 02 03
    yz_zx_xy = _mm_shuffle_ps(yz_zx_xy, yz_zx_xy, _MM_SHUFFLE(0, 0, 2, 0)); // yz_zx_xy = 10 02 10 10
    yz_zx_xy = sseSelect(yz_zx_xy, sseSplat(col2, 1), select_x);            // yz_zx_xy = 21 02 10 10

    sum = _mm_add_ps(zy_xz_yx, yz_zx_xy);
    diff = _mm_sub_ps(zy_xz_yx, yz_zx_xy);
    scale = _mm_mul_ps(invSqrt, _mm_set1_ps(0.5f));

    res0 = _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(0, 1, 2, 0));
    res0 = sseSelect(res0, sseSplat(diff, 0), select_w);
    res1 = _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(0, 0, 0, 2));
    res1 = sseSelect(res1, sseSplat(diff, 1), select_w);
    res2 = _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(0, 0, 0, 1));
    res2 = sseSelect(res2, sseSplat(diff, 2), select_w);
    res3 = diff;
    res0 = sseSelect(res0, radicand, select_x);
    res1 = sseSelect(res1, radicand, select_y);
    res2 = sseSelect(res2, radicand, select_z);
    res3 = sseSelect(res3, radicand, select_w);
    res0 = _mm_mul_ps(res0, sseSplat(scale, 0));
    res1 = _mm_mul_ps(res1, sseSplat(scale, 1));
    res2 = _mm_mul_ps(res2, sseSplat(scale, 2));
    res3 = _mm_mul_ps(res3, sseSplat(scale, 3));

    /* determine case and select answer */
    xx = sseSplat(col0, 0);
    yy = sseSplat(col1, 1);
    zz = sseSplat(col2, 2);
    res = sseSelect(res0, res1, _mm_cmpgt_ps(yy, xx));
    res = sseSelect(res, res2, _mm_and_ps(_mm_cmpgt_ps(zz, xx), _mm_cmpgt_ps(zz, yy)));
    res = sseSelect(res, res3, _mm_cmpgt_ps(sseSplat(diagSum, 0), _mm_setzero_ps()));
    mVec128 = res;
}

// ========================================================
// Misc free functions
// ========================================================

inline const Matrix3 outer(const Vector3 & tfrm0, const Vector3 & tfrm1)
{
    return Matrix3((tfrm0 * tfrm1.getX()),
                   (tfrm0 * tfrm1.getY()),
                   (tfrm0 * tfrm1.getZ()));
}

inline const Matrix3d outer(const Vector3d & tfrm0, const Vector3d & tfrm1)
{
	return Matrix3d((tfrm0 * tfrm1.getX()),
		(tfrm0 * tfrm1.getY()),
		(tfrm0 * tfrm1.getZ()));
}

inline const Matrix4 outer(const Vector4 & tfrm0, const Vector4 & tfrm1)
{
    return Matrix4((tfrm0 * tfrm1.getX()),
                   (tfrm0 * tfrm1.getY()),
                   (tfrm0 * tfrm1.getZ()),
                   (tfrm0 * tfrm1.getW()));
}

inline const Matrix4d outer(const Vector4d & tfrm0, const Vector4d & tfrm1)
{
	return Matrix4d((tfrm0 * tfrm1.getX()),
		(tfrm0 * tfrm1.getY()),
		(tfrm0 * tfrm1.getZ()),
		(tfrm0 * tfrm1.getW()));
}

inline const Vector3 rowMul(const Vector3 & vec, const Matrix3 & mat)
{
    __m128 tmp0, tmp1, mcol0, mcol1, mcol2, res;
    __m128 xxxx, yyyy, zzzz;
    tmp0 = sseMergeH(mat.getCol0().get128(), mat.getCol2().get128());
    tmp1 = sseMergeL(mat.getCol0().get128(), mat.getCol2().get128());
    xxxx = sseSplat(vec.get128(), 0);
    mcol0 = sseMergeH(tmp0, mat.getCol1().get128());
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    mcol1 = _mm_shuffle_ps(tmp0, tmp0, _MM_SHUFFLE(0, 3, 2, 2));
    mcol1 = sseSelect(mcol1, mat.getCol1().get128(), select_y);
    mcol2 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(0, 1, 1, 0));
    mcol2 = sseSelect(mcol2, sseSplat(mat.getCol1().get128(), 2), select_y);
    yyyy = sseSplat(vec.get128(), 1);
    res = _mm_mul_ps(mcol0, xxxx);
    zzzz = sseSplat(vec.get128(), 2);
    res = sseMAdd(mcol1, yyyy, res);
    res = sseMAdd(mcol2, zzzz, res);
    return Vector3(res);
}

inline const Vector3d rowMul(const Vector3d & vec, const Matrix3d & mat)
{
	DSSEVec4 tmp0, tmp1, mcol0, mcol1, mcol2, res;
	DSSEVec4 xxxx, yyyy, zzzz;
	tmp0 = dsseMergeH(mat.getCol0().get256(), mat.getCol2().get256());
	tmp1 = dsseMergeL(mat.getCol0().get256(), mat.getCol2().get256());
	xxxx = dsseSplat(vec.get256(), 0);
	mcol0 = dsseMergeH(tmp0, mat.getCol1().get256());
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	mcol1 = dsseShuffle(tmp0, tmp0, _MM_SHUFFLE(0, 3, 2, 2));
	mcol1 = dsseSelect(mcol1, mat.getCol1().get256(), select_y);
	mcol2 = dsseShuffle(tmp1, tmp1, _MM_SHUFFLE(0, 1, 1, 0));
	mcol2 = dsseSelect(mcol2, dsseSplat(mat.getCol1().get256(), 2), select_y);
	yyyy = dsseSplat(vec.get256(), 1);
	res = dsseMul(mcol0, xxxx);
	zzzz = dsseSplat(vec.get256(), 2);
	res = dsseMAdd(mcol1, yyyy, res);
	res = dsseMAdd(mcol2, zzzz, res);
	return Vector3d(res);
}

inline const Matrix3 crossMatrix(const Vector3 & vec)
{
    __m128 neg, res0, res1, res2;
    neg = sseNegatef(vec.get128());
    VECTORMATH_ALIGNED(unsigned int select_x[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_y[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int select_z[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    res0 = _mm_shuffle_ps(vec.get128(), vec.get128(), _MM_SHUFFLE(0, 2, 2, 0));
    res0 = sseSelect(res0, sseSplat(neg, 1), select_z);
    res1 = sseSelect(sseSplat(vec.get128(), 0), sseSplat(neg, 2), select_x);
    res2 = _mm_shuffle_ps(vec.get128(), vec.get128(), _MM_SHUFFLE(0, 0, 1, 1));
    res2 = sseSelect(res2, sseSplat(neg, 0), select_y);
    VECTORMATH_ALIGNED(unsigned int filter_x[4]) = { 0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
    VECTORMATH_ALIGNED(unsigned int filter_y[4]) = { 0xFFFFFFFF, 0, 0xFFFFFFFF, 0xFFFFFFFF };
    VECTORMATH_ALIGNED(unsigned int filter_z[4]) = { 0xFFFFFFFF, 0xFFFFFFFF, 0, 0xFFFFFFFF };
    res0 = _mm_and_ps(res0, _mm_load_ps((float *)filter_x));
    res1 = _mm_and_ps(res1, _mm_load_ps((float *)filter_y));
    res2 = _mm_and_ps(res2, _mm_load_ps((float *)filter_z));
    return Matrix3(Vector3(res0), Vector3(res1), Vector3(res2));
}

inline const Matrix3d crossMatrix(const Vector3d & vec)
{
	DSSEVec4 neg, res0, res1, res2;
	neg = dsseNegatef(vec.get256());
	VECTORMATH_ALIGNED(unsigned long long select_x[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_y[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0, 0 };
	VECTORMATH_ALIGNED(unsigned long long select_z[4]) = { 0, 0, 0xFFFFFFFFFFFFFFFF, 0 };
	res0 = dsseShuffle(vec.get256(), vec.get256(), _MM_SHUFFLE(0, 2, 2, 0));
	res0 = dsseSelect(res0, dsseSplat(neg, 1), select_z);
	res1 = dsseSelect(dsseSplat(vec.get256(), 0), dsseSplat(neg, 2), select_x);
	res2 = dsseShuffle(vec.get256(), vec.get256(), _MM_SHUFFLE(0, 0, 1, 1));
	res2 = dsseSelect(res2, dsseSplat(neg, 0), select_y);
	VECTORMATH_ALIGNED(unsigned long long filter_x[4]) = { 0, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF };
	VECTORMATH_ALIGNED(unsigned long long filter_y[4]) = { 0xFFFFFFFFFFFFFFFF, 0, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF };
	VECTORMATH_ALIGNED(unsigned long long filter_z[4]) = { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0, 0xFFFFFFFFFFFFFFFF };
	res0 = dsseAnd(res0, dsseLoadu((double *)filter_x));
	res1 = dsseAnd(res1, dsseLoadu((double *)filter_y));
	res2 = dsseAnd(res2, dsseLoadu((double *)filter_z));
	return Matrix3d(Vector3d(res0), Vector3d(res1), Vector3d(res2));
}

inline const Matrix3 crossMatrixMul(const Vector3 & vec, const Matrix3 & mat)
{
    return Matrix3(cross(vec, mat.getCol0()), cross(vec, mat.getCol1()), cross(vec, mat.getCol2()));
}

inline const Matrix3d crossMatrixMul(const Vector3d & vec, const Matrix3d & mat)
{
	return Matrix3d(cross(vec, mat.getCol0()), cross(vec, mat.getCol1()), cross(vec, mat.getCol2()));
}

} // namespace SSE
} // namespace Vectormath

#endif // VECTORMATH_SSE_MATRIX_HPP
