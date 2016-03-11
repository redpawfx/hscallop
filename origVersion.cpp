
#ifdef LINUX
#define DLLEXPORT
#define SIZEOF_VOID_P 8
#else
#define DLLEXPORT __declspec(dllexport)
#endif
#define MAKING_DSO

// CRT
#include <limits.h>
#include <strstream>

#include <iostream>
using namespace std;

// H
#include <UT/UT_DSOVersion.h>

#include <GU/GU_Detail.h>
#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>

#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <PRM/PRM_ChoiceList.h>

#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>

#include <UT/UT_Vector3.h>

class SOP_Scallop :
   public SOP_Node
{
public:
   SOP_Scallop(OP_Network *net, const char *name, OP_Operator *entry) : SOP_Node(net,name,entry) {};
   virtual ~SOP_Scallop() {};

   static OP_Node   *creator(OP_Network  *net, const char *name,   OP_Operator *entry);
   static PRM_Template    templateList[];

protected:
   virtual OP_ERROR    cookMySop   (OP_Context &context);
};

static PRM_Name      bindnames[] =
{
   PRM_Name("obj#", "OBJ Path"),
   PRM_Name("model#", "Model"),
   PRM_Name("weight#", "Weight"),
   PRM_Name("color#", "Color"),
   PRM_Name("daemons",   "Number of Daemons"),
   PRM_Name(0)
};

static PRM_Name      modelNames[] =
{
   PRM_Name("linear",   "Linear"),
   PRM_Name("spherical",    "Spherical"),
   PRM_Name("polar",   "Polar"),
   PRM_Name("swirl",   "Swirl"),
   PRM_Name("trig",   "Trigonometric"),
   PRM_Name(0)
};

static PRM_ChoiceList modelMenu(PRM_CHOICELIST_SINGLE, modelNames);

static PRM_Template theBindTemplates[] =
{
   PRM_Template(PRM_STRING,   PRM_TYPE_DYNAMIC_PATH, 1, &bindnames[0], 0, 0, 0, 0, &PRM_SpareData::objGeometryPath),
   PRM_Template(PRM_ORD,   PRM_Template::PRM_EXPORT_MAX, 1,&bindnames[1], 0, &modelMenu),
   PRM_Template(PRM_FLT, 1, &bindnames[2]),
   PRM_Template(PRM_RGB, 3, &bindnames[3]),
   PRM_Template()
};

static PRM_Default switcherInfo[] = {
   PRM_Default( 2, "Setup"),
   PRM_Default( 1, "Daemons")
};

PRM_Name savePathName("path","Path");
PRM_Default savePathDef(0,"");

PRM_Name countName("count","Count");
PRM_Default countDef(1000,"");

PRM_Name visibleName("showpts","Show Points");
PRM_Name visibleSizeName("ptssz","Point Size");

PRM_Template SOP_Scallop::templateList[]=
{
   PRM_Template(PRM_SWITCHER_EXCLUSIVE,2, &PRMswitcherName, switcherInfo),

   // SPOOL SETUP
   PRM_Template(PRM_FILE,1,&savePathName,&savePathDef),
   PRM_Template(PRM_INT,1,&countName,&countDef),

   // DAEMONS
   PRM_Template(PRM_MULTITYPE_LIST,
   theBindTemplates, 4, &bindnames[4],
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
   UT_Matrix4 xform;
   float c[3];
   float weight;
   float range[2];
   bool Transform(float w, UT_Vector3& P, float* C);
   Daemon() : method(Methods::Linear) { range[0]=0; range[1]=0; };
};

bool Daemon::Transform(float w, UT_Vector3& P, float* C)
{
   if(w<range[0]) return false;
   if(w>=range[1]) return false;

   P = P*xform;

   method(P.vec);

   C[0]=C[0]+0.333*(c[0]-C[0]);
   C[1]=C[1]+0.333*(c[1]-C[1]);
   C[2]=C[2]+0.333*(c[2]-C[2]);

   return true;
};

OP_ERROR SOP_Scallop::cookMySop(OP_Context &context)
{
   //OP_Node::flags().timeDep = 1;

   float now = context.getTime();

   gdp->clearAndDestroy();

   bool showPts = (evalInt("showpts",0,now)!=0);

   if(showPts)
   {
      float sz = evalInt("ptssz",0,now);
      if(sz > 0)
      {
         float one = 1.0f;
         gdp->addAttrib("showpoints",4,GB_ATTRIB_FLOAT,&one);
         gdp->addAttrib("revealsize",4,GB_ATTRIB_FLOAT,&sz);
      };
   };

   int cnt = evalInt("daemons", 0, now);

   //QList<Daemon> daemons;

   Daemon* daemons=new Daemon[cnt];

   float weights = 0;

   int totd=0;

   for(int i=1;i<=cnt;i++)
   {
      Daemon& d = daemons[totd];

      UT_String path = "";
      evalStringInst("obj#", &i, path, 0, now);

      if(path == "") continue;

      SOP_Node* node = getSOPNode(path);

      OBJ_Node* obj = dynamic_cast<OBJ_Node*>(node->getParent());

      if(obj == NULL) continue;

      addExtraInput(obj, OP_INTEREST_DATA);

      d.xform  = obj->getWorldTransform(context);

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
      case 0:
      default:
         d.method = Methods::Linear;
      };

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

   int dt = gdp->addDiffuseAttribute(GEO_POINT_DICT);

   UT_Vector3 current(0,0,0);
   float C[3] = { 0,0,0 };

   srand(0);

   for(int i=-50;i<total;i++)
   {
      bool ok = false;

      float w = double(rand())/double(RAND_MAX);

      for(int j=0;j<totd;j++)
      {
         ok = daemons[j].Transform(w,current,C);
         if(ok) break;
      };

      if(i<0) continue;

      if(ok)
      {
         GEO_Point* p = gdp->appendPoint();
         p->setPos(current);

         float* Cd=p->castAttribData<float>(dt);
         memcpy(Cd,C,12);
      };
   };

   delete [] daemons;

   return error();
}

void newSopOperator(OP_OperatorTable *table)
{
   table->addOperator(new OP_Operator(
      "hdk_scallop","Scallop",
      SOP_Scallop::creator,SOP_Scallop::templateList,
      0,1));
} 
