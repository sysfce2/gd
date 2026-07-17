#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "gd.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @addtogroup TransformScaleRotate
 *
 * Matrix functions to initialize, transform and combine affine matrices used
 * by transform, scale and rotate APIs.
 *
 * They can be used with gdTransformAffineCopy and are also used in various
 * transformation functions in GD.
 *
 * Matrices use a six-element double array:
 *
 * matrix[0] == xx
 * matrix[1] == yx
 * matrix[2] == xy
 * matrix[3] == yy
 * matrix[4] == x0
 * matrix[5] == y0
 *
 * where the transformation of a point (x, y) is:
 *
 * x_new = xx * x + xy * y + x0
 * y_new = yx * x + yy * y + y0
 *
 * @{
 */
BGD_DECLARE(int)
gdAffineApplyToPointF(gdPointFPtr dst, const gdPointFPtr src, const double affine[6])
{
    double x = src->x;
    double y = src->y;
    dst->x = x * affine[0] + y * affine[2] + affine[4];
    dst->y = x * affine[1] + y * affine[3] + affine[5];
    return GD_TRUE;
}

BGD_DECLARE(int) gdAffineInvert(double dst[6], const double src[6])
{
    double r_det = (src[0] * src[3] - src[1] * src[2]);

    if (!isfinite(r_det)) {
        return GD_FALSE;
    }
    if (r_det == 0) {
        return GD_FALSE;
    }

    r_det = 1.0 / r_det;
    dst[0] = src[3] * r_det;
    dst[1] = -src[1] * r_det;
    dst[2] = -src[2] * r_det;
    dst[3] = src[0] * r_det;
    dst[4] = -src[4] * dst[0] - src[5] * dst[2];
    dst[5] = -src[4] * dst[1] - src[5] * dst[3];
    return GD_TRUE;
}

BGD_DECLARE(int)
gdAffineFlip(double dst[6], const double src[6], const int flip_h, const int flip_v)
{
    dst[0] = flip_h ? -src[0] : src[0];
    dst[1] = flip_h ? -src[1] : src[1];
    dst[2] = flip_v ? -src[2] : src[2];
    dst[3] = flip_v ? -src[3] : src[3];
    dst[4] = flip_h ? -src[4] : src[4];
    dst[5] = flip_v ? -src[5] : src[5];
    return GD_TRUE;
}

BGD_DECLARE(int)
gdAffineConcat(double dst[6], const double m1[6], const double m2[6])
{
    double dst0, dst1, dst2, dst3, dst4, dst5;

    dst0 = m1[0] * m2[0] + m1[1] * m2[2];
    dst1 = m1[0] * m2[1] + m1[1] * m2[3];
    dst2 = m1[2] * m2[0] + m1[3] * m2[2];
    dst3 = m1[2] * m2[1] + m1[3] * m2[3];
    dst4 = m1[4] * m2[0] + m1[5] * m2[2] + m2[4];
    dst5 = m1[4] * m2[1] + m1[5] * m2[3] + m2[5];
    dst[0] = dst0;
    dst[1] = dst1;
    dst[2] = dst2;
    dst[3] = dst3;
    dst[4] = dst4;
    dst[5] = dst5;
    return GD_TRUE;
}

BGD_DECLARE(int) gdAffineIdentity(double dst[6])
{
    dst[0] = 1;
    dst[1] = 0;
    dst[2] = 0;
    dst[3] = 1;
    dst[4] = 0;
    dst[5] = 0;
    return GD_TRUE;
}

BGD_DECLARE(int)
gdAffineScale(double dst[6], const double scale_x, const double scale_y)
{
    dst[0] = scale_x;
    dst[1] = 0;
    dst[2] = 0;
    dst[3] = scale_y;
    dst[4] = 0;
    dst[5] = 0;
    return GD_TRUE;
}

BGD_DECLARE(int) gdAffineRotate(double dst[6], const double angle)
{
    const double sin_t = sin(angle * M_PI / 180.0);
    const double cos_t = cos(angle * M_PI / 180.0);

    dst[0] = cos_t;
    dst[1] = sin_t;
    dst[2] = -sin_t;
    dst[3] = cos_t;
    dst[4] = 0;
    dst[5] = 0;
    return GD_TRUE;
}

BGD_DECLARE(int) gdAffineShearHorizontal(double dst[6], const double angle)
{
    dst[0] = 1;
    dst[1] = 0;
    dst[2] = tan(angle * M_PI / 180.0);
    dst[3] = 1;
    dst[4] = 0;
    dst[5] = 0;
    return GD_TRUE;
}

BGD_DECLARE(int) gdAffineShearVertical(double dst[6], const double angle)
{
    dst[0] = 1;
    dst[1] = tan(angle * M_PI / 180.0);
    dst[2] = 0;
    dst[3] = 1;
    dst[4] = 0;
    dst[5] = 0;
    return GD_TRUE;
}

BGD_DECLARE(int)
gdAffineTranslate(double dst[6], const double offset_x, const double offset_y)
{
    dst[0] = 1;
    dst[1] = 0;
    dst[2] = 0;
    dst[3] = 1;
    dst[4] = offset_x;
    dst[5] = offset_y;
    return GD_TRUE;
}

BGD_DECLARE(double) gdAffineExpansion(const double src[6])
{
    return sqrt(fabs(src[0] * src[3] - src[1] * src[2]));
}

BGD_DECLARE(int) gdAffineRectilinear(const double m[6])
{
    return ((fabs(m[1]) < GD_EPSILON && fabs(m[2]) < GD_EPSILON) ||
            (fabs(m[0]) < GD_EPSILON && fabs(m[3]) < GD_EPSILON));
}

BGD_DECLARE(int) gdAffineEqual(const double m1[6], const double m2[6])
{
    return (fabs(m1[0] - m2[0]) < GD_EPSILON && fabs(m1[1] - m2[1]) < GD_EPSILON &&
            fabs(m1[2] - m2[2]) < GD_EPSILON && fabs(m1[3] - m2[3]) < GD_EPSILON &&
            fabs(m1[4] - m2[4]) < GD_EPSILON && fabs(m1[5] - m2[5]) < GD_EPSILON);
}

/** @} */
