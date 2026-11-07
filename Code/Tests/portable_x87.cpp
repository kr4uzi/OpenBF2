// [TEMP] [CHOSEN, not recovered] Scaffolding for the native inline-assembly arms.
// No engine, renderer, game content, sockets or factory registrations are used.
#include "../BF2/Math/PortableX87.h"
#include "../BF2/Math/Mat4.h"
#include <climits>

#pragma STDC FENV_ACCESS ON

namespace {
int checks;
void check(bool value, int line)
{
    ++checks;
    if (!value) { std::fprintf(stderr, "portable_x87.cpp:%d failed\n", line); std::abort(); }
}
#define CHECK(value) check((value), __LINE__)
float fromBits(std::uint32_t bits) { float value; std::memcpy(&value,&bits,4); return value; }
std::uint32_t bits(float value) { std::uint32_t result; std::memcpy(&result,&value,4); return result; }
void same(float actual, float expected)
{
    CHECK((std::isnan(actual) && std::isnan(expected)) || bits(actual)==bits(expected));
}

#if defined(__x86_64__) && defined(__clang__)
// [TEMP] Host x87 oracle for the native math library; not original-game evidence.
// These GNU blocks use native-width addresses and never enter production code.
long double oracleCosine(long double angle)
{
    long double result;
    asm volatile("fldt %1; fcos; fstpt %0" : "=m"(result) : "m"(angle) : "st");
    return result;
}
long double oracleSine(long double angle)
{
    long double result;
    asm volatile("fldt %1; fsin; fstpt %0" : "=m"(result) : "m"(angle) : "st");
    return result;
}
int oracleInteger(float value)
{
    int result;
    asm volatile("flds %1; frndint; fistpl %0" : "=m"(result) : "m"(value) : "st");
    return result;
}
float oracleMultiply(float factor, float alpha)
{
    float result;
    asm volatile("flds %1; fmuls %2; fstps %0" : "=m"(result) : "m"(factor), "m"(alpha) : "st");
    return result;
}
#endif
}

int main()
{
    using namespace bf2x87;
    const int rounding = std::fegetround();
    CHECK(std::fesetround(FE_TONEAREST)==0);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const float large = std::numeric_limits<float>::max();
    const float samples[] = {nan,0.0f,-0.0f,inf,-inf,0.5f,-0.5f,1.5f,2.5f,large,-large};

    // FISTP invalid produces the signed minimum, never a C++ out-of-range cast.
    CHECK(integer<int>(nan)==INT_MIN);
    CHECK(integer<int>(inf)==INT_MIN);
    CHECK(integer<int>(-inf)==INT_MIN);
    CHECK(integer<int>(large)==INT_MIN);
    CHECK(integer<int>(2147483648.0L)==INT_MIN);
    CHECK(integer<int>(-2147483648.0L)==INT_MIN);
    CHECK(integer<int>(2147483647.9L)==INT_MAX);
    CHECK(integer<int>(-2147483648.9L)==INT_MIN);
    CHECK(integer<std::int64_t>(std::ldexp(1.0L,63))==LLONG_MIN);
    CHECK(integer<std::int64_t>(wide(LLONG_MAX))==LLONG_MAX);
    CHECK(integer<short>(32768)==SHRT_MIN);
    CHECK(integer<int>(1.5L)==1 && integer<int>(-1.5L)==-1);
    CHECK(integer<int>(std::floor(-1.5L))==-2);
    CHECK(integer<int>(std::ceil(1.5L))==2);
    CHECK(integer<int>(0.5L,false)==0 && integer<int>(-0.5L,false)==0);
    CHECK(integer<int>(1.5L,false)==2 && integer<int>(2.5L,false)==2);
    CHECK(integer<int>(-1.5L,false)==-2 && integer<int>(-2.5L,false)==-2);
    CHECK(std::fesetround(FE_DOWNWARD)==0);
    CHECK(integer<int>(1.5L,false)==1 && integer<int>(-1.5L,false)==-2);
    CHECK(integer<int>(-1.5L)==-1 && std::fegetround()==FE_DOWNWARD);
    CHECK(std::fesetround(FE_UPWARD)==0);
    CHECK(integer<int>(1.5L,false)==2 && integer<int>(-1.5L,false)==-1);
    CHECK(std::fesetround(FE_TOWARDZERO)==0);
    CHECK(integer<int>(1.5L,false)==1 && integer<int>(-1.5L,false)==-1);
    CHECK(std::fesetround(FE_TONEAREST)==0);
    CHECK(lowWord(0x12345678ffffffffLL)==-1 && lowShort(0xffff)==-1);

    CHECK(notLess(nan,0) && greaterOrUnordered(nan,0));
    std::feclearexcept(FE_ALL_EXCEPT);
    CHECK(notLess(nan,0) && std::fetestexcept(FE_INVALID));
    CHECK(notLess(0,-0.0L) && !greaterOrUnordered(0,-0.0L));
    same(store(decoded(-2)),1); same(store(decoded(-1)),-0.0f);
    same(store(decoded(-0.0f)),-0.0f);
    CHECK(notLess(decoded(nan),1));
    CHECK(sumNotLess(nan,1,2));
    CHECK(sumNotLess(1,1,2));
    CHECK(!sumNotLess(1,1,3));
    // No float/double spill before cancellation or the final comparison.
    same(offset(1,16777216.0f,16777216.0f),1);
    same(offset(1,large,large),0);
    CHECK(sumNotLess(16777216.0f,1,16777216.0f));
    same(center(10,3),8.5f);
    same(multiplyAdd(16777216.0f,1.00000011920928955078125f,-16777216.0f),2);
    same(multiply(-0.0f,1),-0.0f);
    CHECK(bits(multiply(fromBits(0x7fc00001),fromBits(0x7fc00002)))==0x7fc00002);
    CHECK(bits(multiply(fromBits(0x7f800001),fromBits(0x7fc00002)))==0x7fc00002);
    CHECK(bits(multiply(fromBits(0x7fc00001),fromBits(0x7f800002)))==0x7fc00001);

    same(fade(0,1,1,false),0); same(fade(1,1,1,true),1);
    same(fade(-0.0f,0,1,false),-0.0f);
    float value=37;
    CHECK(!chooseValue(nan,1,1,1,0,false,value) && value==37);
    CHECK(!chooseValue(1,1,1,1,0,true,value) && value==37);
    CHECK(chooseValue(0,1,2,1,0,false,value) && value==1);
    CHECK(chooseValue(2,1,2,1,0,false,value) && value==1);
    CHECK(chooseValue(0,1,nan,1,0,false,value) && std::isnan(value));
    CHECK(chooseValue(0,2,1,1,0,true,value) && value==1);
    CHECK(chooseValue(2,0,1,1,0,true,value) && value==1);
    CHECK(chooseValue(0,1,1,1,2,true,value) && value>0 && value<1);
    CHECK(chooseValue(1,0,1,1,2,true,value) && value>0 && value<1);
    char text[16]; char* cursor=text;
    fractionalDigits(1.25,1,3,cursor); *cursor=0;
    CHECK(std::strcmp(text,"250")==0);
    cursor=text; fractionalDigits(inf,0,3,cursor); *cursor=0;
    CHECK(std::strcmp(text,"000")==0);
    cursor=text; fractionalDigits(nan,0,3,cursor); *cursor=0;
    CHECK(std::strcmp(text,"000")==0);

    double remaining=37;
    // The binary64 1/60 constant is slightly low, so the retained product
    // leaves a positive wait even at exactly sixty seconds.
    CHECK(waitRemaining(0,1,60,remaining) && remaining>0 && remaining<1e-15);
    CHECK(waitRemaining(0,1,0,remaining) && remaining==1);
    CHECK(waitRemaining(0,nan,0,remaining) && std::isnan(remaining));
    CHECK(!abilityPosition(-4000,-4000)); CHECK(abilityPosition(nan,-4000));
    float duration=2, left=0, deadline=0; bool active=false;
    beginCooldown(1,duration,left,active,deadline,true);
    CHECK(left==2 && active && deadline==3);
    active=false; beginCooldown(2,duration,left,active,deadline,false);
    CHECK(left==2 && active && deadline==4);
    float position[3]={1,2,3};
    movePointer(position,1,-1,2,true,100,60); CHECK(position[0]==50 && position[1]==-30);
    movePointer(position,1,-1,2,false,100,60); CHECK(position[0]==52 && position[1]==-32);
    value=-1; clampCoordinate(value,0,2); CHECK(value==0);
    value=3; clampCoordinate(value,0,2); CHECK(value==2);
    value=nan; clampCoordinate(value,0,2); CHECK(std::isnan(value));
    CHECK(affine(1,2,3,4,5,6,7)==51);
    float x,y,z; project(2,3,2,100,50,x,y,z); CHECK(x==50 && y==-75 && z==0.5f);
    same(store(div(wide(1),wide(2))),0.5f);
    CHECK(distanceWithX(wide(3.0f),0,0,4,0,2)==10);
    CHECK(distance(0,0,0,3,4,0,2)==10);
    pullPoint(0,0,0,0,0,0,position); CHECK(position[0]==52);
    pullPoint(1,0,0,0,0,0,position); same(position[0],0.8f); CHECK(position[1]==0 && position[2]==0);
    value=0.5f; CHECK(subtractTimer(value,0.5f,true) && value==0);
    value=0.5f; CHECK(!subtractTimer(value,0.5f,false));
    value=nan; CHECK(!subtractTimer(value,1,true) && std::isnan(value));
    value=9.5f; CHECK(addDemoTimer(value,0.5f) && value==10);
    value=nan; CHECK(!addDemoTimer(value,1));
    CHECK(mapCoordinate(100,10,0.01f,true)==std::nextafter(0.6f,0.0f));
    CHECK(mapCoordinate(100,10,0.01f,false)==0.5f);
    same(heading(0,1),0); same(heading(-0.0f,1),-0.0f);
    same(heading(1,0),1.57079632679489661923f);
    same(heading(0,-1),3.14159265358979323846f);
    CHECK(std::isnan(heading(nan,1)));
    CHECK(armorRatio(1,2,1,false)==0.5f && armorRatio(1,2,1,true)==0.75f);
    same(ammoRatio(15,0,false),1.0f); same(ammoRatio(15,0,true),0.5f);
    float fraction; cooldown(10,2,4,left,fraction); CHECK(left==8 && fraction==2);
    CHECK(glyphWidth(1,2,3,4)==10);
    float co; cofactor(co,1,2,3,4,5,6,1,1,1,1,1,1); CHECK(co==41);
    float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float inverse[16]; std::memcpy(inverse,matrix,sizeof(matrix));
    divideByDeterminant(matrix,inverse);
    for (int i=0;i<16;++i) same(inverse[i],matrix[i]);
    dice::hfe::Mat4 original, inverted;
    for (int i=0;i<16;++i) original.m[i]=matrix[i];
    original.m[0]=2; original.m[5]=4; original.m[10]=8;
    original.m[12]=6; original.m[13]=8; original.m[14]=16;
    dice::hfe::fullInvert(original,inverted);
    CHECK(inverted.m[0]==0.5f && inverted.m[5]==0.25f && inverted.m[10]==0.125f);
    CHECK(inverted.m[12]==-3 && inverted.m[13]==-2 && inverted.m[14]==-2);
    dice::hfe::fullInvert(original,original);
    for (int i=0;i<16;++i) same(original.m[i],inverted.m[i]);
    float base,near,inverseNear,far,inverseFar;
    lod(2,100,0.25f,0.5f,base,near,inverseNear,far,inverseFar);
    CHECK(base==200 && near==50 && far==100); same(inverseNear,1.0f/150); same(inverseFar,0.01f);
    CHECK(effectiveDistance(nan,1,1)==200 && effectiveDistance(200,1,1)==200);
    CHECK(effectiveDistance(300,1,1)==300);
    float s,c; trig(-0.0f,s,c); same(s,-0.0f); same(c,1);
    trig(large,s,c); CHECK(bits(s)==0xffc00000U && c==large);
    CHECK(sine(wide(large))==wide(large) && cosine(wide(large))==wide(large));
    short si,ci; packedRotation(0,si,ci); CHECK(si==0 && ci==32767);
    tableRotation(0,si,ci); CHECK(si==0 && ci==32767);
    tableRotation(1024,si,ci); CHECK(si==32766 && ci==0);
    packedRotation(1.57079632679489661923f,si,ci); CHECK(si==32766 && ci==0);
    packedRotation(-1.57079632679489661923f,si,ci); CHECK(si==-32766 && ci==0);
    packedRotation(nan,si,ci); CHECK(si==0 && ci==0);
    float zoomValue,inverseZoom; zoom(1,0.5f,1,zoomValue,inverseZoom); CHECK(zoomValue==1 && inverseZoom==1);
    zoom(inf,0.5f,1,zoomValue,inverseZoom); CHECK(std::isnan(zoomValue) && std::isnan(inverseZoom));
    bool flag=false; value=fromBits(0x7f800001);
    std::feclearexcept(FE_ALL_EXCEPT);
    same(consoleFloat(&value,&flag),nan); CHECK(flag && std::fetestexcept(FE_INVALID));

    // Exercise every scalar family with NaN, signed zeros, infinities, halves,
    // and extreme magnitudes. Expected cases are stated above; here check the
    // shared unordered and invalid-conversion invariants across all samples.
    for (float sample:samples) {
        same(multiply(sample,1),sample);
        same(offset(sample,0,0),std::isnan(sample) ? nan : sample==0 ? 0.0f : sample);
        same(center(sample,0),sample);
        same(multiplyAdd(sample,1,0),sample==0 ? 0.0f : sample);
        CHECK(sumNotLess(sample,0,0)==!(sample<0));
        same(fade(sample,0,1,true),sample>1 ? 1.0f : sample==0 ? 0.0f : sample);
        same(fade(sample,0,1,false),sample<0 ? 0.0f : sample);
        value=37; CHECK(chooseValue(sample,sample,1,1,1,true,value)==false && value==37);
        same(armorRatio(sample,1,0,false),sample);
        same(affine(sample,1,0,1,0,1,0),sample==0 ? 0.0f : sample);
        flag=false; same(consoleFloat(&sample,&flag),sample); CHECK(flag);
        float a,b; trig(sample,a,b);
        if (std::isnan(sample)||std::isinf(sample)) CHECK(std::isnan(a)&&std::isnan(b));
        cooldown(sample,0,1,a,b); same(a,sample); same(b,sample);
        // These blocks use the same sample through independent expression
        // families, checking their caller-visible stores and branch decisions.
        project(sample,sample,1,2,1,x,y,z);
        same(x,sample); same(y,-sample); same(z,1);
        same(distance(0,0,0,sample,0,0,1),std::fabs(sample));
        same(distanceWithX(wide(sample),0,0,0,0,1),std::fabs(sample));
        same(glyphWidth(sample,0,0,0),sample==0 ? 0.0f : sample);
        cofactor(co,sample,1,0,1,0,1,0,1,0,1,0,1);
        same(co,sample==0 ? 0.0f : sample);
        same(mapCoordinate(2,sample,1,true),std::isnan(sample) ? nan : sample==0 ? 1.0f : sample+1.0f);
        same(effectiveDistance(sample,1,1),sample>200 ? sample : 200.0f);
        same(ammoRatio(0,sample,true),sample==0 ? 0.0f : sample*0.5f);
        value=37;
        const bool denied=waitRemaining(0,sample,0,remaining);
        CHECK(denied==(!(sample<=0)));
        if (denied) same(static_cast<float>(remaining),sample);
        bool busy=false; float remainingField=37,deadlineField=37;
        beginCooldown(0,sample,remainingField,busy,deadlineField,true);
        same(remainingField,sample); same(deadlineField,sample==0 ? 0.0f : sample); CHECK(busy);
        beginCooldown(0,sample,remainingField,busy,deadlineField,false);
        same(remainingField,sample); same(deadlineField,sample==0 ? 0.0f : sample); CHECK(busy);
        lod(sample,1,0.25f,0.5f,base,near,inverseNear,far,inverseFar);
        same(base,sample); same(near,sample*0.25f); same(far,sample*0.5f);
        position[0]=sample; position[1]=sample;
        movePointer(position,0,0,1,false,1,1);
        same(position[0],sample==0 ? 0.0f : sample); same(position[1],sample==0 ? 0.0f : sample);
        movePointer(position,sample,sample,1,true,2,2);
        same(position[0],sample); same(position[1],sample);
        value=sample; clampCoordinate(value,-1,2);
        same(value,sample < -1 ? -1.0f : sample>1 ? 1.0f : sample);
        value=sample; CHECK(subtractTimer(value,0,false)==(sample<0));
        same(value,sample);
        value=sample; CHECK(addDemoTimer(value,0)==(sample>=10));
        same(value,sample==0 ? 0.0f : sample);
        CHECK(abilityPosition(sample,-4000));
        packedRotation(sample,si,ci);
        if (std::isnan(sample)||std::isinf(sample)||std::fabs(sample)>=std::ldexp(1.0f,63))
            CHECK(si==0 && ci==0);
    }
    CHECK(std::fesetround(rounding)==0);
#if defined(__x86_64__) && defined(__clang__)
    // Verify the extended results, not only their binary32 stores. Darwin's
    // sinl/cosl agree with the host x87 instructions for these samples.
    CHECK(std::fesetround(FE_TONEAREST)==0);
#if !defined(BF2_TEST_SOFT_EXTENDED)
    std::uint32_t random=0;
    for (int i=0;i<10000;++i) {
        random=random*1664525U+1013904223U;
        const long double angle=mul(wide(lowWord(random)),0.00000001L);
        CHECK(cosine(angle)==oracleCosine(angle));
        CHECK(sine(angle)==oracleSine(angle));
    }
#endif
    const std::uint32_t alphaCases[]={0x7fc00001,0xffc00001,0x7fc00002,0x7f800001,0xff800002,0,0x80000000,0x7f800000,0xff800000,0x3f800000};
    for (auto first:alphaCases) for (auto second:alphaCases)
        CHECK(bits(multiply(fromBits(first),fromBits(second)))==bits(oracleMultiply(fromBits(first),fromBits(second))));
    const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    for (int mode:modes) {
        CHECK(std::fesetround(mode)==0);
        for (float sample:samples) CHECK(integer<int>(wide(sample),false)==oracleInteger(sample));
    }
    CHECK(std::fesetround(rounding)==0);
#endif
    std::printf("portable x87 arms: %d checks passed\n",checks);
}
