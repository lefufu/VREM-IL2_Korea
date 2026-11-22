//****************************************
// VREM Uniform variable handled by VREM

//MASK texture 
texture MaskBuffetTex : MASK;
sampler<float4> MaskBuffer { Texture = MaskBuffetTex; };

// depth texture
texture DepthBufferTex : DEPTH;
sampler DepthBuffer { Texture = DepthBufferTex; };

//stencil
texture StencilBufferTex : STENCIL;
sampler<uint4> StencilBuffer { Texture = StencilBufferTex; };

// variable pushed by VREM here
uniform float unif_MSAA;

#define AIRFRAME 1
#define COCKPIT 2
#define SKY 3
#define WATER 4
#define GROUND 5
#define PLANE 6
#define SIGHT 7


int mak_value(float4 mask, float4 depth)
{
    //aiframe
    int value = 0;

    if (mask.y > 0.1 && mask.y <= 0.4 && mask.w == 0)
    {
        value = AIRFRAME;	
        if (depth.x < 0.001)
            value = PLANE;
    } 
    // cockpit
    else  if (mask.y > 0.4 && mask.w == 0 && mask.z < 0.9)
        value = COCKPIT;
    // sky
    else if (mask.x == 0 && mask.y == 0 && mask.z == 0 && mask.w == 0)
    {
        value = SKY;
        //P38 mirror on engine 
        //if (depth.x > 0.008)
        //    value = AIRFRAME;
    }
    //water 
    else if (mask.y > 0 && mask.w == 1.0)
        value = WATER;
    else
        //ground
       value = GROUND;

    // cockpit mirror
	//if (depth.x > 0.015)
	if (depth.x > 0.09)
        value = COCKPIT;

    return value;
}

bool is_cockpit(float2 coord)
{
    bool check = false;
	
	float2 coord2;
	coord2.x = coord.x;
	coord2.y = coord.y;
	
	uint sampledData = tex2Dlod(MaskBuffer, float4(coord2, 0, 0)).g;
	
	if (sampledData == 40)
        check = true;
	
	return check;
}

bool is_sight(float2 coord)
{
    bool check = false;
	
	float2 coord2;
	coord2.x = coord.x;
	coord2.y = coord.y;
	
	uint sampledData = tex2Dlod(StencilBuffer, float4(coord2, 0, 0)).g;
	
	if (sampledData > 4)
        check = true;
	
	return check;
}


/*

// target for quad view => 0 : all, 1 Outer, 2 Innner
uniform int VREMQuadViewTarget <
    ui_category = "VREM Settings";
	ui_items = 
    "All views\0"
	"QV Outer\0"
    "QV Inner\0"
	;
    ui_label = "Quad view targets";
    ui_tooltip = "Define if technique will be rendered on all views, or only on outer views or only in Inner views. Works only if HMD is quad view";
    ui_type = "combo";
> = 0;

*/