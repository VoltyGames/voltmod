#include <mathlib/vector.h>
#include <tier0/utlvector.h>

float S2SdkHeadersCompile()
{
    Vector v(1.0f, 2.0f, 3.0f);
    CUtlVector<int> values;
    values.AddToTail(static_cast<int>(v.Length()));
    return v.Length() + static_cast<float>(values.Count());
}
