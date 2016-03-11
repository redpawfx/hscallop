/*************************************************************
* Copyright (c) 2012 by Egor N. Chashchin. All Rights Reserved.          *
**************************************************************/

/*
*       main.cpp - Scallop - system for generating and visualization of attraction areas
*       of stochastic non-linear attractors
*
*       Version: 0.99
*       Authors: Egor N. Chashchin
*       Contact: iqcook@gmail.com
*
*/
#ifdef LINUX
#define DLLEXPORT
#define SIZEOF_VOID_P 8
#else
#define DLLEXPORT __declspec(dllexport)
#endif

//#define MAKING_DSO
#define SESI_LITTLE_ENDIAN 1

// CRT
#include <sys/stat.h>

#include <limits.h>
#include <strstream>

#include <iostream>
using namespace std;

// H
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVolume.h>
#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>

#include <PRM/PRM_Include.h>
#include <PRM/PRM_Template.h>
#include <PRM/PRM_SpareData.h>
#include <PRM/PRM_ChoiceList.h>

#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_Caller.h>

#include <UT/UT_Vector3.h>
#include <UT/UT_Ramp.h>
#include <UT/UT_Interrupt.h>

#include <VEX/VEX_Error.h>
#include <CVEX/CVEX_Context.h>
#include <CVEX/CVEX_Value.h>

#include <SHOP/SHOP_Node.h>

//////////////////////////////////////////////////////////////////////////

int thecallbackfuncdiv(void *data, int index, float time,const PRM_Template *tplate);
int thecallbackfuncdata(void *data, int index, float time,const PRM_Template *tplate);

#define STR_PARM(str, name, idx, vi, t) { evalString(str, name, &ifdIndirect[idx], vi, (float)t); }
#define INT_PARM(name, idx, vi, t) { return evalInt(name, &ifdIndirect[idx], vi, t); }
#define FLT_PARM(name, idx, vi, t) { return evalFloat(name, &ifdIndirect[idx], vi, t); }

class SOP_Scallop :
        public SOP_Node
{
public:
        SOP_Scallop(OP_Network *net, const char *name, OP_Operator *entry) : SOP_Node(net,name,entry)
        {
                if (!ifdIndirect) ifdIndirect = allocIndirect(256);
        };
        virtual ~SOP_Scallop() {};

        static OP_Node  *creator(OP_Network  *net, const char *name,    OP_Operator *entry);
        static PRM_Template      templateList[];

        void SaveDivMap(float time);
        void SaveData(float time);

protected:
        virtual OP_ERROR         cookMySop   (OP_Context &context);

        static int* ifdIndirect;
};

int* SOP_Scallop::ifdIndirect = 0;

static PRM_Name         bindnames[] =
{
        PRM_Name("enabled#", "Enabled"),
        PRM_Name("obj#", "OBJ Path"),
        PRM_Name("model#", "Model"),
        PRM_Name("weight#", "Weight"),
        PRM_Name("color#", "Color"),
        PRM_Name("power#", "Power"),
        PRM_Name("vexcode#", "CVEX"),
        PRM_Name("radius#", "Radius"),
        PRM_Name("parameter#", "Parameter"),
        PRM_Name("daemons",     "Number of Daemons"),
        PRM_Name(0)
};

static PRM_Name         modelNames[] =
{
        PRM_Name("linear",      "Linear"),
        PRM_Name("spherical",   "Spherical"),
        PRM_Name("polar",       "Polar"),
        PRM_Name("swirl",       "Swirl"),
        PRM_Name("trig",        "Trigonometric"),
        PRM_Name("vex", "VEX"),
        PRM_Name(0)
};

static PRM_ChoiceList modelMenu(PRM_CHOICELIST_SINGLE, modelNames);

static PRM_Template theBindTemplates[] =
{
        PRM_Template(PRM_TOGGLE,        1, &bindnames[0], PRMoneDefaults),
        PRM_Template(PRM_STRING,        PRM_TYPE_DYNAMIC_PATH, 1, &bindnames[1], 0, 0, 0, 0, &PRM_SpareData::objGeometryPath),
        PRM_Template(PRM_ORD,   PRM_Template::PRM_EXPORT_MAX, 1,&bindnames[2], 0, &modelMenu),
        PRM_Template(PRM_FLT, 1, &bindnames[3],PRMoneDefaults),
        PRM_Template(PRM_RGB, 3, &bindnames[4]),
        PRM_Template(PRM_FLT, 1, &bindnames[5],PRMoneDefaults),
        PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH,PRM_Template::PRM_EXPORT_MAX, 1,
        &bindnames[6], 0, 0, 0, 0,&PRM_SpareData::shopCVEX),
        PRM_Template(PRM_FLT, 1, &bindnames[7],PRMoneDefaults),
        PRM_Template(PRM_FLT, 1, &bindnames[8],PRMzeroDefaults),
        PRM_Template()
};

PRM_Name savePathName("path","Path");
PRM_Default savePathDef(0,"");

PRM_Name mapPathName("mappath","Map Path");
PRM_Default mapPathDef(0,"");

PRM_Name mapMedialName("mapmedial","Use medial Division");

PRM_Name mapDivName("mapdiv","Finite Division");

PRM_Name countName("count","Count");
PRM_Default countDef(1000,"");

PRM_Name degrName("degr","Display Degradation");
PRM_Default degrDef(5,"");

PRM_Name visibleName("showpts","Show Points");
PRM_Name visibleSizeName("ptssz","Point Size");

PRM_Name radiiName("trackradii","Track Radius");
PRM_Name radiiScaleName("radiiscale","Radii Scale");

PRM_Name biasName("bias","Parameter Bias");
PRM_Default biasDef(0.5f,"");

static PRM_Name btnSpoolDivBgeo("spooldivbgeo", "Save Division Map");
static PRM_Name nodeCount("nodecount","Division Node Count");
static PRM_Name btnSpoolData("spoolscdata", "Save Data");

// RAMP
static PRM_Name SpValue ("spvalue", "Point value");
static PRM_Name SpOffset ("spoffset", "Point offset");
static PRM_Name SpInterp ("spinterp", "Point interpolation");

static PRM_Name paramnetricColorName("parmcolor", "Parametric Color");
static PRM_Name Ramp ("ramp", "Color Ramp");

static PRM_Template RampTemplates[] = {
        PRM_Template (PRM_FLT_J, 1, &SpOffset, PRMzeroDefaults),
        PRM_Template (PRM_FLT_J, 1, &SpValue, PRMzeroDefaults),
        PRM_Template (PRM_INT, 1, &SpInterp, PRMtwoDefaults),
        PRM_Template (),
};

static PRM_Default switcherInfo[] = {
        PRM_Default( 14, "Setup"),
        PRM_Default( 1, "Daemons")
};

PRM_Template SOP_Scallop::templateList[]=
{
        PRM_Template(PRM_SWITCHER_EXCLUSIVE,2, &PRMswitcherName, switcherInfo),

        // SPOOL SETUP
        PRM_Template(PRM_INT,1,&countName,&countDef),
        PRM_Template(PRM_INT,1,&degrName,&degrDef),
        PRM_Template(PRM_TOGGLE,1,&radiiName,PRMoneDefaults),
        PRM_Template(PRM_FLT,1,&radiiScaleName,PRMoneDefaults),
        PRM_Template(PRM_FLT,1,&biasName,&biasDef),
        PRM_Template(PRM_TOGGLE,1,&paramnetricColorName,PRMzeroDefaults),
        PRM_Template (PRM_MULTITYPE_RAMP_RGB, NULL, 1, &Ramp, PRMtwoDefaults),
        PRM_Template(PRM_CALLBACK,      1, &btnSpoolData, NULL, NULL, NULL,thecallbackfuncdata),
        PRM_Template(PRM_FILE,1,&savePathName,&savePathDef),
        PRM_Template(PRM_CALLBACK,      1, &btnSpoolDivBgeo, NULL, NULL, NULL,thecallbackfuncdiv),
        PRM_Template(PRM_INT,1,&nodeCount,PRM100Defaults),
        PRM_Template(PRM_FILE,1,&mapPathName,&mapPathDef),
        PRM_Template(PRM_TOGGLE,1,&mapMedialName,PRMzeroDefaults),
        PRM_Template(PRM_INT,1,&mapDivName, PRMtenDefaults),

        // DAEMONS
        PRM_Template(PRM_MULTITYPE_LIST,
        theBindTemplates, 9, &bindnames[9],
        PRMzeroDefaults, 0, &PRM_SpareData::multiStartOffsetOne),

        // RECENT
        PRM_Template(PRM_TOGGLE,1,&visibleName,PRMoneDefaults),
        PRM_Template(PRM_INT,1,&visibleSizeName,PRMtwoDefaults),
        PRM_Template()
};

OP_Node *SOP_Scallop::creator(OP_Network *net,const char *name,OP_Operator *entry)
{
        return new SOP_Scallop(net, name, entry);
};

struct Methods
{
        static void Linear(float*) {};
        static void Spherical(float*_P)
        {
                float Len = sqrt(_P[0]*_P[0]+_P[1]*_P[1]+_P[2]*_P[2]);
                if(Len!=0) { _P[0] /= Len; _P[1] /= Len; _P[2] /= Len+_P[1]; };
        };
        static void Polar(float*_P)
        {
                float Len = sqrt(_P[0]*_P[0]+_P[1]*_P[1]+_P[2]*_P[2]);
                _P[0] = atan2(_P[0],_P[1]); _P[1] = Len;
        };
        static void Swirl(float*_P)
        {
                float Len = sqrt(_P[0]*_P[0]+_P[1]*_P[1]+_P[2]*_P[2]);
                _P[0] = Len*cos(Len); _P[1] = Len*sin(Len);
        };
        static void Trigonometric(float*_P)
        {
                _P[2] = cos(abs(_P[2]+_P[0]))-abs(sin(_P[2]+_P[1]));
                _P[0] = cos(_P[0]);
                _P[1] = sin(_P[1]);
        };
};

typedef void  (*NonLinear)(float*);

struct Daemon
{
        NonLinear method;
        UT_DMatrix4 xform;
        float c[3];
        float weight;
        float range[2];
        float power;
        UT_String vexsrc;
        bool Transform(float w, UT_Vector3& P, float* C, float& R, float& p);
        void SetupCVEX(UT_String script);
        bool useVex;
        CVEX_Context context;
        float in[3];
        float out[3];
        float radius;
        float parameter;

        Daemon() : method(Methods::Linear), power(1.0f), useVex(false), radius(1.0f), parameter(0.0f) { range[0]=0; range[1]=0; };
        ~Daemon();

        static float now;
        static OP_Node* caller;
        static float bias;
        static bool useColor;
};

float Daemon::now=0.0f;
OP_Node* Daemon::caller=NULL;
float Daemon::bias=0.5f;
bool Daemon::useColor = true;

void Daemon::SetupCVEX(UT_String script)
{
        vexsrc=script;

        SHOP_Node *shop = caller->findSHOPNode(vexsrc);
        caller->addExtraInput(shop, OP_INTEREST_DATA);

        shop->buildVexCommand(vexsrc, shop->getSpareParmTemplates(), now);
        shop->buildShaderString(script, now, 0);

        UT_String op = "op:";
        op += vexsrc;
        vexsrc=op;

        char *argv[4096];
        int argc = vexsrc.parse(argv, 4096);

        //OP_Caller       C(caller);
        //context.setOpCaller(&C);
        //context.setTime(now);
        context.addInput("P",CVEX_TYPE_VECTOR3,true);

        context.load(argc, argv );
        useVex = context.isLoaded();

        if(useVex)
        {
                CVEX_Value* inv = context.findInput("P",CVEX_TYPE_VECTOR3);
                inv->setData(in,1);

                CVEX_Value* outv = context.findOutput("P",CVEX_TYPE_VECTOR3);
                outv->setData(out,1);
        };
};

bool Daemon::Transform(float w, UT_Vector3& P, float* C, float& R, float& p)
{
        if(w<range[0]) return false;
        if(w>=range[1]) return false;

        UT_Vector3 _P=P;

        _P = _P*xform;

        if(useVex)
        {
                memcpy(in,_P.vec,12);
                context.run(1,false);
                memcpy(_P.vec,out,12);
        }
        else method(_P.vec);

        if(useColor)
        {
                C[0]=C[0]+power*0.333*(c[0]-C[0]);
                C[1]=C[1]+power*0.333*(c[1]-C[1]);
                C[2]=C[2]+power*0.333*(c[2]-C[2]);
        };

        P = P+power*(_P-P);

        R += power*(radius-R);

        p += bias*(parameter-p);

        return true;
};

Daemon::~Daemon() {};

OP_ERROR SOP_Scallop::cookMySop(OP_Context &context)
{
        //OP_Node::flags().timeDep = 1;

        bool clip = (lockInputs(context) < UT_ERROR_ABORT);

        UT_BoundingBox bbox;

        if(clip)
        {
                const GU_Detail* input = inputGeo(0,context);
                if(input != NULL)
                {
                        //UT_Matrix4 bm;
                        int res = input->getBBox(&bbox);
                        if(res == 0) clip = false;
                }
                else clip = false;
                unlockInputs();
        };

        float now = context.getTime();

        Daemon::now=now;
        Daemon::caller=this;

        Daemon::bias = evalFloat("bias",0,now);

        UT_Ramp ramp;
        float   rampout[4];

        bool useRamp = (evalInt("parmcolor",0,now)!=0);

        if(useRamp)
        {
                //PRM_Template *rampTemplate = PRMgetRampTemplate ("ramp", PRM_MULTITYPE_RAMP_RGB, NULL, NULL);
                if (ramp.getNodeCount () < 2)
                {
                        ramp.addNode (0, UT_FRGBA (0, 0, 0, 1));
                        ramp.addNode (1, UT_FRGBA (1, 1, 1, 1));
                };
                updateRampFromMultiParm(now, getParm("ramp"), ramp);
        };

        gdp->clearAndDestroy();

        bool showPts = (evalInt("showpts",0,now)!=0);

		/*
        if(showPts)
        {
                float sz = evalInt("ptssz",0,now);
                if(sz > 0)
                {
                        float one = 1.0f;

                        gdp->addAttribute("showpoints",4,GA_ATTRIB_FLOAT_&one);
                        gdp->addAttribute("revealsize",4,GB_ATTRIB_FLOAT,&sz);
                };
        };
		*/

        int cnt = evalInt("daemons", 0, now);

        Daemon* daemons=new Daemon[cnt];

        float weights = 0;

        int totd=0;

        for(int i=1;i<=cnt;i++)
        {
                bool skip = (evalIntInst("enabled#",&i,0,now)==0);
                if(skip) continue;

                Daemon& d = daemons[totd];

                UT_String path = "";
                evalStringInst("obj#", &i, path, 0, now);

                if(path == "") continue;

                SOP_Node* node = getSOPNode(path);

                OBJ_Node* obj = dynamic_cast<OBJ_Node*>(node->getParent());

                if(obj == NULL) continue;

                addExtraInput(obj, OP_INTEREST_DATA);

                //d.xform  = obj->getWorldTransform(context); // 10.0
                obj->getWorldTransform(d.xform, context);

                d.weight = evalFloatInst("weight#",&i,0,now);

                if(!useRamp)
                {
                        d.c[0] = evalFloatInst("color#",&i,0,now);
                        d.c[1] = evalFloatInst("color#",&i,1,now);
                        d.c[2] = evalFloatInst("color#",&i,2,now);
                };

                int mth = evalIntInst("model#",&i,0,now);

                switch(mth)
                {
                case 1:
                        d.method = Methods::Spherical;
                        break;
                case 2:
                        d.method = Methods::Polar;
                        break;
                case 3:
                        d.method = Methods::Swirl;
                        break;
                case 4:
                        d.method = Methods::Trigonometric;
                        break;
                case 5:
                        {
                                UT_String script;
                                evalStringInst("vexcode#", &i, script, 0, now);
                                d.SetupCVEX(script);
                                if(d.useVex)
                                {
                                        OP_Node* shop = (OP_Node*)findSHOPNode(script);
                                        addExtraInput(shop, OP_INTEREST_DATA);
                                }
                                break;
                        }
                case 0:
                default:
                        d.method = Methods::Linear;
                };

                d.power = evalFloatInst("power#",&i,0,now);
                d.radius = evalFloatInst("radius#",&i,0,now);
                d.parameter = evalFloatInst("parameter#",&i,0,now);

                weights+=d.weight;
                totd++;
        };

        if(totd == 0)
        {
                delete [] daemons;
                return error();
        }

        float base = 0.0;
        for(int i=0;i<totd;i++)
        {
                Daemon& d = daemons[i];
                d.range[0]=base;
                d.range[1] = base+d.weight/weights;
                base=d.range[1];
        };

        int total = evalInt("count",0,now);
        int degr = evalInt("degr",0,now);

        total >>= degr;

		GA_RWHandleI cntt(gdp->addIntTuple(GA_ATTRIB_POINT, "count", 4, GA_Defaults(1.0)));


        GB_AttributeRef dt(gdp->addDiffuseAttribute(GEO_POINT_DICT));
        gdp->addVariableName("Cd","Cd");

        UT_Vector3 current(0,0,0);
        float C[3] = { 0,0,0 };

        float R=1.0f;
        bool trackRadii = (evalInt("trackradii",0,now)!=0);
        float rScale = evalFloat("radiiscale",0,now);
        GB_AttributeRef rt;
        if(trackRadii)
        {
                float one=1.0f;
                rt = gdp->addPointAttrib("width",4,GB_ATTRIB_FLOAT,&one);
                if(!GBisAttributeRefValid(rt)) trackRadii=false;
                else gdp->addVariableName("width","WIDTH");
        };

        float zero=0.0f;
        GB_AttributeRef pt = gdp->addPointAttrib("parameter",4,GB_ATTRIB_FLOAT,&zero);
        if(GBisAttributeRefValid(pt)) gdp->addVariableName("parameter","PARAMETER");
        float param=0.0f;

        srand(0);

        UT_Interrupt* boss = UTgetInterrupt();
        boss->opStart("Computing...");

        for(int i=-50;i<total;i++)
        {
                bool ok = false;

                if (boss->opInterrupt()) break;

                float w = double(rand())/double(RAND_MAX);

                for(int j=0;j<totd;j++)
                {
                        ok = daemons[j].Transform(w,current,C,R,param);
                        if(ok) break;
                };

                if(i<0) continue;

                if(clip)
                {
                        if(!bbox.isInside(current)) continue;
                };

                if(ok)
                {
                        GEO_Point* p = gdp->appendPoint();
                        p->setPos(current);

                        float* Cd=p->castAttribData<float>(dt);
                        if(useRamp)
                        {
                                ramp.rampLookup(param,C);
                        }
                        memcpy(Cd,C,12);

                        if(trackRadii)
                        {
                                float* _R = p->castAttribData<float>(rt);
                                *_R=rScale*R;
                        };

                        if(GBisAttributeRefValid(pt))
                        {
                                float* _p = p->castAttribData<float>(pt);
                                *_p=param;
                        }
                };
        };

        boss->opEnd();

        delete [] daemons;

        return error();
};

static float data[4];
static float* G = data+3;

int thecallbackfuncdiv(void *data, int index, float time,const PRM_Template *tplate)
{
        SOP_Scallop *me = (SOP_Scallop *) data;
        me->SaveDivMap(time);
        return 0;
};

#define QUANT 65536

struct PointList
{
        float* data;
        int limit;
        int count;
        int chsize;
        void add(float*);
        void clear();
        float* operator[](int i);
        PointList(int _limit)
                : limit(_limit)
                , count(0)
                , data(NULL)
                , chsize(0)
        {};
        ~PointList() { if(data != NULL) delete [] data;         };
};
void PointList::add(float* p)
{
        if(count == limit) return;
        if(count == chsize)
        {
                int dev = limit-chsize;
                if(dev > QUANT) dev = QUANT;
                float* ch = new float[3*(chsize+dev)];
                memset(ch,0,12*(chsize+dev));
                memcpy(ch,data,12*chsize);
                chsize+=dev;
                delete [] data;
                data = ch;
        };
        float* d = data + count*3;
        memcpy(d,p,12);
        count++;
};
void PointList::clear()
{
        delete [] data;
        count = 0;
        chsize = 0;
        data = NULL;
};
float* PointList::operator[](int i)
{
        if(i>=count) return NULL;
        return data+3*i;
};

#define NODECOUNT 8
class BoundBox
{
public:
        BoundBox() :
                organized(false)
                , splitted(false)
                , b(NULL)
                , p(limit)
                , count(0)
                {};
        ~BoundBox()
        {
                if(b != NULL) delete [] b;
        };

        static int limit;
        static float radius;
        static bool medial;

        bool organized;
        float bounds[6];
        void Organize(float* B)
        {
                memcpy(bounds,B,24);
                organized = true;
        };

        bool CheckPoint(float* P);
        bool splitted;
        BoundBox* b;

        void Split();
        void Centroid(float*);

        void Build(GU_Detail& gdp);
        void Collapse();

        PointList p;
        int count;
};

int BoundBox::limit = -1;
float BoundBox::radius = 0;
bool BoundBox::medial = false;

bool BoundBox::CheckPoint(float* P)
{
        if(P[0]<bounds[0]) return false;
        if(P[0]>=bounds[3]) return false;
        if(P[1]<bounds[1]) return false;
        if(P[1]>=bounds[4]) return false;
        if(P[2]<bounds[2]) return false;
        if(P[2]>=bounds[5]) return false;

        p.limit = limit;

        if(!splitted)
        {
                p.add(P);

                count++;

                if(count < limit) return true;

                Split();
                for(int i=0;i<count;i++)
                {
                        float* _P = p[i];
                        for(int j=0;j<NODECOUNT;j++)
                        {
                                if(b[j].CheckPoint(_P)) break;
                        };
                };

                p.clear();

                return true;
        };

        for(int i=0;i<NODECOUNT;i++)
        {
                if(b[i].CheckPoint(P)) break;
        };

        return true;
};
void BoundBox::Split()
{
        float C[3];
        Centroid(C);
        b = new BoundBox[8];

        {
                float B0[6] = {bounds[0],bounds[1],bounds[2], C[0],C[1],C[2]};
                b[0].Organize(B0);
        }

        {
                float B1[6] = {C[0],bounds[1],bounds[2], bounds[3],C[1],C[2]};
                b[1].Organize(B1);
        }

        {
                float B2[6] = {bounds[0],C[1],bounds[2], C[0],bounds[4],C[2]};
                b[2].Organize(B2);
        }

        {
                float B3[6] = {bounds[0],bounds[1],C[2], C[0],C[1],bounds[5]};
                b[3].Organize(B3);
        }

        {
                float B4[6] = {bounds[0],C[1],C[2],C[0],bounds[4],bounds[5]};
                b[4].Organize(B4);
        }

        {
                float B5[6] = {C[0],bounds[1],C[2],bounds[3],C[1],bounds[5]};
                b[5].Organize(B5);
        }

        {
                float B6[6] = {C[0],C[1],bounds[2],bounds[3],bounds[4],C[2]};
                b[6].Organize(B6);
        }

        {
                float B7[6] = {C[0],C[1],C[2],bounds[3],bounds[4],bounds[5]};
                b[7].Organize(B7);
        }

        // ...
        splitted = true;
};
void BoundBox::Centroid(float* C)
{
        // ...SIMPLEST IMPLEMENTATION
        C[0]=0; C[1]=0; C[2]=0;
        if(medial)
        {
                C[0]=bounds[0]+bounds[3]; C[0]/=2;
                C[1]=bounds[1]+bounds[4]; C[1]/=2;
                C[2]=bounds[2]+bounds[5]; C[2]/=2;
        }
        else
        {
                for(int i=0;i<count;i++)
                {
                        float* P=p[i];
                        C[0]+=P[0];C[1]+=P[1];C[2]+=P[2];
                }
                C[0]/=count; C[1]/=count; C[2]/=count;
        }
};

float func(const UT_Vector3&) { return 0.0f; }
void BoundBox::Build(GU_Detail& gdp)
{
        if(!organized) return;
        if(!splitted)
        {
                if(count < 1) return;

                GB_AttributeRef at = gdp.addPrimAttrib("count",4,GB_ATTRIB_INT,&count);

                GB_AttributeRef rt = gdp.addAttrib("radius",4,GB_ATTRIB_FLOAT,&radius);

                // SETUP BBOX
                Collapse();
                UT_BoundingBox bbox(bounds[0],bounds[1],bounds[2],bounds[3],bounds[4],bounds[5]);

                int div[3] = {1,1,1};
                GEO_Primitive* pl = GU_PrimVolume::buildFromFunction(&gdp,func,bbox,div[0],div[1],div[2]);

                if(GBisAttributeRefValid(at))
                {
                        int* ct = pl->castAttribData<int>(at);
                        *ct = count;
                };

                return;
        };

        for(int i=0;i<NODECOUNT;i++) b[i].Build(gdp);
};

void BoundBox::Collapse()
{
        float B[6] = { 10000000,10000000,10000000,-10000000,-10000000,-10000000 };
        for(int i=0;i<count;i++)
        {
                float* P = p[i];

                if(P[0]<B[0]) B[0]=P[0];
                if(P[0]>B[3]) B[3]=P[0];

                if(P[1]<B[1]) B[1]=P[1];
                if(P[1]>B[4]) B[4]=P[1];

                if(P[2]<B[2]) B[2]=P[2];
                if(P[2]>B[5]) B[5]=P[2];
        };

        memcpy(bounds,B,24);
};

struct OctreeBox
{
        OctreeBox(int _l) : level(_l), C(NULL), filled(false), count(0), radius(0.0f) {};

        bool Insert(float*P);

        ~OctreeBox();

        UT_BoundingBox bbox;

        void Build(GU_Detail& gdp);

        static GB_AttributeRef at, rt, it, bt;

        static UT_BoundingBox parentbbox;
        static int maxlevel;

private:
        OctreeBox() : C(NULL), filled(false), count(0), radius(0.0f) {};

        int level;
        OctreeBox* C;

        bool filled;

        int count;
        float radius;

        void Split();
};

GB_AttributeRef OctreeBox::at;
GB_AttributeRef OctreeBox::rt;
GB_AttributeRef OctreeBox::it;
GB_AttributeRef OctreeBox::bt;

int OctreeBox::maxlevel = 1;
UT_BoundingBox OctreeBox::parentbbox;

OctreeBox::~OctreeBox()
{
        if(C!=NULL) delete [] C;
};

bool OctreeBox::Insert(float*P)
{
        UT_Vector3 p(P);
        if(!bbox.isInside(p)) return false;

        filled = true;

        if(level != 0)
        {
                if(C == NULL) Split();
                for(int i=0;i<8;i++)
                {
                        if(C[i].Insert(P)) break;
                };
        }
        else
        {
                if(P[3] > radius) radius = P[3];
                count++;
        };

        return true;
};

void OctreeBox::Split()
{
        C = new OctreeBox[8];
        for(int i=0;i<8;i++)
        {
                C[i].level=level-1;
        };

        UT_Vector3 c = bbox.center();
        UT_Vector3 m = bbox.minvec();
        UT_Vector3 M = bbox.maxvec();

        C[0].bbox = UT_BoundingBox(m.vec[0],m.vec[1],m.vec[2],c.vec[0],c.vec[1],c.vec[2]);

        C[1].bbox = UT_BoundingBox(c.vec[0],m.vec[1],m.vec[2],M.vec[0],c.vec[1],c.vec[2]);

        C[2].bbox = UT_BoundingBox(m.vec[0],c.vec[1],m.vec[2],c.vec[0],M.vec[1],c.vec[2]);

        C[3].bbox = UT_BoundingBox(m.vec[0],m.vec[1],c.vec[2],c.vec[0],c.vec[1],M.vec[2]);

        C[4].bbox = UT_BoundingBox(m.vec[0],c.vec[1],c.vec[2],c.vec[0],M.vec[1],M.vec[2]);

        C[5].bbox = UT_BoundingBox(c.vec[0],m.vec[1],c.vec[2],M.vec[0],c.vec[1],M.vec[2]);

        C[6].bbox = UT_BoundingBox(c.vec[0],c.vec[1],m.vec[2],M.vec[0],M.vec[1],c.vec[2]);

        C[7].bbox = UT_BoundingBox(c.vec[0],c.vec[1],c.vec[2],M.vec[0],M.vec[1],M.vec[2]);
};

void OctreeBox::Build(GU_Detail& gdp)
{
        if(!filled) return;

        if(level == 0)
        {
                //if(count < 1) return;
                int div[3] = {1,1,1};
                GEO_Primitive* pl = GU_PrimVolume::buildFromFunction(&gdp,func,bbox,div[0],div[1],div[2]);

                if(GBisAttributeRefValid(at))
                {
                        int* ct = pl->castAttribData<int>(at);
                        *ct = count;
                };

                if(GBisAttributeRefValid(rt))
                {
                        float* ct = pl->castAttribData<float>(rt);
                        *ct = radius;
                };

                if(GBisAttributeRefValid(it))
                {
                        int* ct = pl->castAttribData<int>(it);
                        ct[0] = float(bbox.centerX()-parentbbox.xmin())/parentbbox.sizeX()*maxlevel;
                        ct[1] = float(bbox.centerY()-parentbbox.ymin())/parentbbox.sizeY()*maxlevel;
                        ct[2] = float(bbox.centerZ()-parentbbox.zmin())/parentbbox.sizeZ()*maxlevel;
                };

        }
        else
        {
                for(int i=0;i<8;i++) C[i].Build(gdp);
        }

        return;
};

//////////////////////////////////////////////////////////////////////////
void SOP_Scallop::SaveDivMap(float time)
{
        OP_Context context(time);

        bool clip = (lockInputs(context) < UT_ERROR_ABORT);

        UT_BoundingBox bbox;

        if(clip)
        {
                const GU_Detail* input = inputGeo(0,context);
                if(input != NULL)
                {
                        //UT_Matrix4 bm;
                        int res = input->getBBox(&bbox);
                        if(res == 0) clip = false;
                }
                else clip = false;
                unlockInputs();
        };

        if(!clip) return;

        UT_String file;
        STR_PARM(file,"mappath", 11, 0, time);

        float& now=time;
        //////////////////////////////////////////////////////////////////////////

        Daemon::now=now;
        Daemon::bias = evalFloat("bias",0,now);

        int cnt = evalInt("daemons", 0, now);

        Daemon* daemons=new Daemon[cnt];

        float weights = 0;

        int totd=0;

        float maxR = 0;
        for(int i=1;i<=cnt;i++)
        {
                bool skip = (evalIntInst("enabled#",&i,0,now)==0);
                if(skip) continue;

                Daemon& d = daemons[totd];

                UT_String path = "";
                evalStringInst("obj#", &i, path, 0, now);

                if(path == "") continue;

                SOP_Node* node = getSOPNode(path);

                OBJ_Node* obj = dynamic_cast<OBJ_Node*>(node->getParent());

                if(obj == NULL) continue;

                obj->getWorldTransform(d.xform,context);

                d.weight = evalFloatInst("weight#",&i,0,now);

                d.c[0] = evalFloatInst("color#",&i,0,now);
                d.c[1] = evalFloatInst("color#",&i,1,now);
                d.c[2] = evalFloatInst("color#",&i,2,now);

                int mth = evalIntInst("model#",&i,0,now);

                switch(mth)
                {
                case 1:
                        d.method = Methods::Spherical;
                        break;
                case 2:
                        d.method = Methods::Polar;
                        break;
                case 3:
                        d.method = Methods::Swirl;
                        break;
                case 4:
                        d.method = Methods::Trigonometric;
                        break;
                case 5:
                        {
                                UT_String script;
                                evalStringInst("vexcode#", &i, script, 0, now);
                                d.SetupCVEX(script);

                                break;
                        };
                case 0:
                default:
                        d.method = Methods::Linear;
                };

                d.power = evalFloatInst("power#",&i,0,now);
                d.radius = evalFloatInst("radius#",&i,0,now);
                d.parameter = evalFloatInst("parameter#",&i,0,now);

                if(d.radius > maxR) maxR = d.radius;

                weights+=d.weight;
                totd++;
        };

        if(totd == 0)
        {
                delete [] daemons;
                return;
        };

        float base = 0.0;
        for(int i=0;i<totd;i++)
        {
                Daemon& d = daemons[i];
                d.range[0]=base;
                d.range[1] = base+d.weight/weights;
                base=d.range[1];
        };

        //////////////////////////////////////////////////////////////////////////
        int total = evalInt("count",0,now);
        int degr = evalInt("degr",0,now);

        total >>= degr;

        GU_Detail det;

        UT_Vector3 current(0,0,0);
        float C[3] = { 0,0,0 };

        float R=1.0f;

        float param=0.0f;

        srand(0);

        bool medial = (evalInt("mapmedial",0,now)!=0);
        int mapdiv = evalInt("mapdiv",0,now);

        //BoundBox Box;
        OctreeBox O(mapdiv);

        //if(medial)
        //{
                O.bbox=bbox;
        //}
        //else
        //{
        //      BoundBox::limit = evalInt("nodecount", 0, now);

        //      BoundBox::medial = (evalInt("mapmedial",0,now)!=0);

        //      float boxb[6];
        //      memcpy(boxb,bbox.minvec().vec,12);
        //      memcpy(boxb+3,bbox.maxvec().vec,12);
        //      Box.Organize(boxb);
        //};

        for(int i=-50;i<total;i++)
        {
                bool ok = false;

                float w = double(rand())/double(RAND_MAX);

                for(int j=0;j<totd;j++)
                {
                        ok = daemons[j].Transform(w,current,C,R,param);
                        if(ok) break;
                };

                if(i<0) continue;

                //if(medial)
                //{
                        float P[4] = { current.x(), current.y(), current.z(), R };
                        O.Insert(P);
                //}
                //else
                //{
                //      Box.CheckPoint(current.vec);
                //}
        };

        delete [] daemons;

        //////////////////////////////////////////////////////////////////////////

        int ita[3] = {-1,-1,-1};

        //if(medial)
        //{
                int count = 0;
                OctreeBox::at = det.addPrimAttrib("count",4,GB_ATTRIB_INT,&count);
                det.addVariableName("count","COUNT");

                float radius = 0.0f;
                OctreeBox::rt = det.addAttrib("radius",4,GB_ATTRIB_FLOAT,&radius);
                det.addVariableName("radius","RADIUS");

                OctreeBox::it = det.addPrimAttrib("mask",12,GB_ATTRIB_INT,ita);
                det.addVariableName("mask","MASK");

                float box[6] = {bbox.xmin(),bbox.xmax(),bbox.ymin(),bbox.ymax(),bbox.zmin(),bbox.zmax()};
                det.addAttrib("bbox",24,GB_ATTRIB_FLOAT,box);

                O.maxlevel = 0x01<<mapdiv;
                O.parentbbox = bbox;

                O.Build(det);
        //}
        //else  Box.Build(det);

        det.save(file.buffer(),1,NULL);

        // ...SAVE ATLAS

        {
                UT_String atlas =file;
                atlas+=".atlas";
                FILE* fa = fopen(atlas.buffer(),"wb");

                GEO_PrimList& pl = det.primitives();

                int cnt = pl.entries();

                fwrite(&cnt,sizeof(int),1,fa);

                float bb[6] = { bbox.xmin(), bbox.xmax(), bbox.ymin(), bbox.ymax(), bbox.zmin(), bbox.zmax() };
                fwrite(bb,sizeof(float),6,fa);

                fwrite(&(O.maxlevel),sizeof(int),1,fa);
                fwrite(&(O.maxlevel),sizeof(int),1,fa);
                fwrite(&(O.maxlevel),sizeof(int),1,fa);

                for(int i=0;i<cnt;i++)
                {
                        const GEO_PrimVolume* v = dynamic_cast<const GEO_PrimVolume*>(pl[i]);
                        UT_BoundingBox b;
                        v->getBBox(&b);
                        float _bb[6] = { b.xmin(), b.xmax(), b.ymin(), b.ymax(), b.zmin(), b.zmax() };
                        fwrite(_bb,sizeof(float),6,fa);

                        // MASK
                        fwrite(v->castAttribData<int>(OctreeBox::it),sizeof(int),3,fa);
                }

                fclose(fa);
        }
};

int thecallbackfuncdata(void *data, int index, float time,const PRM_Template *tplate)
{
        SOP_Scallop *me = (SOP_Scallop *) data;
        me->SaveData(time);
        return 0;
};

void SOP_Scallop::SaveData(float time)
{
        OP_Context context(time);

        bool clip = (lockInputs(context) < UT_ERROR_ABORT);

        UT_BoundingBox bbox;

        if(clip)
        {
                const GU_Detail* input = inputGeo(0,context);
                if(input != NULL)
                {
                        int res = input->getBBox(&bbox);
                        if(res == 0) clip = false;
                }
                else clip = false;
                unlockInputs();
        };

        UT_String file;
        STR_PARM(file,"path", 8, 0, time);

        FILE* fp = fopen(file.buffer(),"wb");

        if(fp == NULL) return;

        float& now=time;
        //////////////////////////////////////////////////////////////////////////

        UT_Ramp ramp;
        float   rampout[4];

        bool useRamp = (evalInt("parmcolor",0,now)!=0);

        if(useRamp)
        {
                //PRM_Template *rampTemplate = PRMgetRampTemplate ("ramp", PRM_MULTITYPE_RAMP_RGB, NULL, NULL);
                if (ramp.getNodeCount () < 2)
                {
                        ramp.addNode (0, UT_FRGBA (0, 0, 0, 1));
                        ramp.addNode (1, UT_FRGBA (1, 1, 1, 1));
                };
                updateRampFromMultiParm(now, getParm("ramp"), ramp);
        };

        Daemon::now=now;

        Daemon::bias = evalFloat("bias",0,now);

        int cnt = evalInt("daemons", 0, now);

        Daemon* daemons=new Daemon[cnt];

        float weights = 0;

        int totd=0;

        for(int i=1;i<=cnt;i++)
        {
                bool skip = (evalIntInst("enabled#",&i,0,now)==0);
                if(skip) continue;

                Daemon& d = daemons[totd];

                UT_String path = "";
                evalStringInst("obj#", &i, path, 0, now);

                if(path == "") continue;

                SOP_Node* node = getSOPNode(path);

                OBJ_Node* obj = dynamic_cast<OBJ_Node*>(node->getParent());

                if(obj == NULL) continue;

                obj->getWorldTransform(d.xform,context);

                d.weight = evalFloatInst("weight#",&i,0,now);

                d.c[0] = evalFloatInst("color#",&i,0,now);
                d.c[1] = evalFloatInst("color#",&i,1,now);
                d.c[2] = evalFloatInst("color#",&i,2,now);

                int mth = evalIntInst("model#",&i,0,now);

                switch(mth)
                {
                case 1:
                        d.method = Methods::Spherical;
                        break;
                case 2:
                        d.method = Methods::Polar;
                        break;
                case 3:
                        d.method = Methods::Swirl;
                        break;
                case 4:
                        d.method = Methods::Trigonometric;
                        break;
                case 5:
                        {
                                UT_String script;
                                evalStringInst("vexcode#", &i, script, 0, now);
                                d.SetupCVEX(script);

                                break;
                        }
                case 0:
                default:
                        d.method = Methods::Linear;
                };

                d.power = evalFloatInst("power#",&i,0,now);
                d.radius = evalFloatInst("radius#",&i,0,now);
                d.parameter = evalFloatInst("parameter#",&i,0,now);

                weights+=d.weight;
                totd++;
        };

        if(totd == 0)
        {
                delete [] daemons;
                return;
        }

        float base = 0.0;
        for(int i=0;i<totd;i++)
        {
                Daemon& d = daemons[i];
                d.range[0]=base;
                d.range[1] = base+d.weight/weights;
                base=d.range[1];
        };

        int total = evalInt("count",0,now);

        //fwrite(&total,sizeof(int),1,fp);

        UT_Vector3 current(0,0,0);
        float* C = data;

        float R=1.0f;
        float rScale = evalFloat("radiiscale",0,now);

        float param=0.0f;

        srand(0);

        for(int i=-50;i<total;i++)
        {
                bool ok = false;

                float w = double(rand())/double(RAND_MAX);

                for(int j=0;j<totd;j++)
                {
                        ok = daemons[j].Transform(w,current,C,R,*G);
                        if(ok) break;
                };

                if(i<0) continue;

                if(clip)
                {
                        if(!bbox.isInside(current)) continue;
                };

                if(ok)
                {
                        if(useRamp)
                        {
                                float out[4];
                                ramp.rampLookup(data[3],out);
                                memcpy(data,out,12);
                        }
                        fwrite(current.vec,12,1,fp); // P
                        float r = R*rScale;
                        fwrite(&r,4,1,fp); // R
                        fwrite(data,16,1,fp); // Cs+p
                };
        };

        delete [] daemons;

        //////////////////////////////////////////////////////////////////////////

        fclose(fp);
};

//////////////////////////////////////////////////////////////////////////

void newSopOperator(OP_OperatorTable *table)
{
        table->addOperator(new OP_Operator(
                "hdk_scallop","Scallop",
                SOP_Scallop::creator,SOP_Scallop::templateList,
                0,1));
};

#include <UT/UT_DSOVersion.h>
