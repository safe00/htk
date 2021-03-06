/* ----------------------------------------------------------- */
/*                                                             */
/*                          ___                                */
/*                       |_| | |_/   SPEECH                    */
/*                       | | | | \   RECOGNITION               */
/*                       =========   SOFTWARE                  */ 
/*                                                             */
/*                                                             */
/* ----------------------------------------------------------- */
/*         Copyright: Microsoft Corporation                    */
/*          1995-2000 Redmond, Washington USA                  */
/*                    http://www.microsoft.com                 */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*         File: HERest.c: Embedded B-W ReEstimation           */
/* ----------------------------------------------------------- */

char *herest_version = "!HVER!HERest:   3.0 [CUED 05/09/00]";
char *herest_vc_id = "$Id: HERest.c,v 1.4 2000/09/11 13:53:33 ge204 Exp $";

/*
   This program is used to perform a single reestimation of
   the parameters of a set of HMMs using Baum-Welch.  Training
   data consists of one or more utterances each of which has a 
   transcription in the form of a standard label file (segment
   boundaries are ignored).  For each training utterance, a
   composite model is effectively synthesised by concatenating
   the phoneme models given by the transcription.  Each phone
   model has the usual set of accumulators allocated to it,
   these are updated by performing a standard B-W pass over
   each training utterance using the composite model. This program
   supports arbitrary parameter tying and multiple data streams.
   
   Added in V1.4 - support for tee-Models ie HMMs with a non-
   zero transition from entry to exit states.

   In v2.2 most of the core functionality has been moved to the
   library module HFB
*/

#include "HShell.h"     /* HMM ToolKit Modules */
#include "HMem.h"
#include "HMath.h"
#include "HSigP.h"
#include "HAudio.h"
#include "HWave.h"
#include "HVQ.h"
#include "HParm.h"
#include "HLabel.h"
#include "HModel.h"
#include "HTrain.h"
#include "HUtil.h"
#include "HAdapt.h"
#include "HFB.h"

/* Trace Flags */
#define T_TOP   0001    /* Top level tracing */
#define T_MAP   0002    /* logical/physical hmm map */
#define T_MIX   0004    /* Mixture Weights */
#define T_UPD   0010    /* Model updates */

/* Global Settings */

static char * labDir = NULL;     /* label (transcription) file directory */
static char * labExt = "lab";    /* label file extension */
static char * hmmDir = NULL;     /* directory to look for hmm def files */
static char * hmmExt = NULL;     /* hmm def file extension */
static char * newDir = NULL;     /* directory to store new hmm def files */
static char * newExt = NULL;     /* extension of new reestimated hmm files */
static char * statFN;            /* stats file, if any */
static float minVar  = 0.0;      /* minimum variance (diagonal only) */
static float mixWeightFloor=0.0; /* Floor for mixture weights */
static int minEgs    = 3;        /* min examples to train a model */

static UPDSet uFlags = (UPDSet) (UPMEANS|UPVARS|UPTRANS|UPMIXES); /* update flags */
static int parMode   = -1;       /* enable one of the // modes */
static Boolean stats = FALSE;    /* enable statistics reports */
static char * mmfFn  = NULL;     /* output MMF file, if any */
static int trace     = 0;        /* Trace level */
static Boolean saveBinary = FALSE;  /* save output in binary  */
static Boolean ldBinary = TRUE;        /* load/dump in binary */
static FileFormat dff=UNDEFF;       /* data file format */
static FileFormat lff=UNDEFF;       /* label file format */

static ConfParam *cParm[MAXGLOBS];   /* configuration parameters */
static int nParm = 0;               /* total num params */
Boolean traceHFB = FALSE;        /* pass to HFB to retain top-level tracing */

/* Global Data Structures - valid for all training utterances */
static LogDouble pruneInit = NOPRUNE;    /* pruning threshold initially */
static LogDouble pruneInc = 0.0;         /* pruning threshold increment */
static LogDouble pruneLim = NOPRUNE;     /* pruning threshold limit */
static float minFrwdP = 10.0;    /* mix prune threshold */


static Boolean firstTime = TRUE;    /* Flag used to enable creation of ot */
static Boolean twoDataFiles = FALSE; /* Enables creation of ot2 for FB
                                        training using two data files */
static int totalT=0;       /* total number of frames in training data */
static LogDouble totalPr;   /* total log prob upto current utterance */
static Vector vFloor[SMAX]; /* variance floor - default is all zero */

static MemHeap hmmStack;   /*For Storage of all dynamic structures created...*/
static MemHeap uttStack;
static MemHeap fbInfoStack;

/* ------------------ Process Command Line -------------------------- */
   
/* SetConfParms: set conf parms relevant to HCompV  */
void SetConfParms(void)
{
   int i;
   Boolean b;
   
   nParm = GetConfig("HEREST", TRUE, cParm, MAXGLOBS);
   if (nParm>0) {
      if (GetConfInt(cParm,nParm,"TRACE",&i)) trace = i;
      if (GetConfBool(cParm,nParm,"SAVEBINARY",&b)) saveBinary = b;
      if (GetConfBool(cParm,nParm,"BINARYACCFORMAT",&b)) ldBinary = b;
   }
}

void ReportUsage(void)
{
   printf("\nUSAGE: HERest [options] hmmList dataFiles...\n\n");
   printf(" Option                                   Default\n\n");
   printf(" -c f    Mixture pruning threshold          10.0\n");
   printf(" -d s    dir to find hmm definitions       current\n");
   printf(" -m N    set min examples needed per model   3\n");
   printf(" -o s    extension for new hmm files        as src\n");
   printf(" -p N    set parallel mode to N             off\n");
   printf(" -r      Enable Single Pass Training...      \n");
   printf("         ...using two parameterisations     off\n");
   printf(" -s s    print statistics to file s         off\n");
   printf(" -t f [i l] set pruning to f [inc limit]    inf\n");
   printf(" -u tmvw update t)rans m)eans v)ars w)ghts  tmvw\n");
   printf(" -v f    set minimum variance to f          0.0\n");
   printf(" -w f    set mix weight floor to f*MINMIX   0.0\n");
   printf(" -x s    extension for hmm files            none\n");
   PrintStdOpts("BFGHILMSTX");
   printf("\n\n");
}

void SetuFlags(void)
{
   char *s;
   
   s=GetStrArg();
   uFlags=(UPDSet) 0;        
   while (*s != '\0')
      switch (*s++) {
      case 't': uFlags = (UPDSet) (uFlags+UPTRANS); break;
      case 'm': uFlags = (UPDSet) (uFlags+UPMEANS); break;
      case 'v': uFlags = (UPDSet) (uFlags+UPVARS); break;
      case 'w': uFlags = (UPDSet) (uFlags+UPMIXES); break;
      default: HError(2320,"SetuFlags: Unknown update flag %c",*s);
         break;
      }
}

/* ScriptWord: return next word from script */
char *ScriptWord(FILE *script, char *scriptBuf)
{
   int ch,qch,i;
   
   i=0; ch=' ';
   while (isspace(ch)) ch = fgetc(script);
   if (ch==EOF) {
      scriptBuf=NULL;
      return NULL;
   }
   if (ch=='\'' || ch=='"'){
      qch = ch;
      ch = fgetc(script);
      while (ch != qch && ch != EOF) {
         scriptBuf[i++] = ch; 
         ch = fgetc(script);
      }
      if (ch==EOF)
         HError(5051,"ScriptWord: Closing quote missing in script file");
   } else {
      do {
         scriptBuf[i++] = ch; 
         ch = fgetc(script);
      }while (!isspace(ch) && ch != EOF);
   }
   scriptBuf[i] = '\0';

   return scriptBuf;
}

int main(int argc, char *argv[])
{
   char *datafn=NULL;
   char *datafn2=NULL;
   char *s;
   char *scriptFile;
   char datafn1[MAXSTRLEN];
   char newFn[MAXSTRLEN];
   FILE *f;
   UttInfo *utt;            /* utterance information storage */
   FBInfo *fbInfo;          /* forward-backward information storage */
   HMMSet hset;             /* Set of HMMs to be re-estimated */
   Source src;
   float tmpFlt;
   int tmpInt;
   int numUtt;

   void Initialise(FBInfo *fbInfo, MemHeap *x, HMMSet *hset, char *hmmListFn);
   void DoForwardBackward(FBInfo *fbInfo, UttInfo *utt, char *datafn, char *datafn2);
   void UpdateModels(HMMSet *hset, ParmBuf pbuf2);
   void StatReport(HMMSet *hset);
   
   if(InitShell(argc,argv,herest_version,herest_vc_id)<SUCCESS)
      HError(2300,"HERest: InitShell failed");
   InitMem();    InitMath();
   InitSigP();   InitAudio();
   InitWave();   InitVQ();
   InitLabel();
   if(InitParm()<SUCCESS)  
      HError(2300,"HERest: InitParm failed");
   InitModel();  InitTrain();
   InitUtil();   InitFB();

   if (!InfoPrinted() && NumArgs() == 0)
      ReportUsage();
   if (NumArgs() == 0) Exit(0);

   SetConfParms();
   CreateHeap(&hmmStack,"HmmStore", MSTAK, 1, 1.0, 50000, 500000);
   CreateHMMSet(&hset,&hmmStack,TRUE);
   CreateHeap(&uttStack,   "uttStore",    MSTAK, 1, 0.5, 100,   1000);
   utt = (UttInfo *) New(&uttStack, sizeof(UttInfo));
   CreateHeap(&fbInfoStack,   "FBInfoStore",  MSTAK, 1, 0.5, 100 ,  1000 );
   fbInfo = (FBInfo *) New(&fbInfoStack, sizeof(FBInfo));

   while (NextArg() == SWITCHARG) {
      s = GetSwtArg();
      if (strlen(s)!=1) 
         HError(2319,"HERest: Bad switch %s; must be single letter",s);
      switch(s[0]){
      case 'b':
         if (NextArg()!=STRINGARG)
            HError(2319,"HERest: script file expected");
         scriptFile = GetStrArg(); break;
      case 'c':
         minFrwdP = GetChkedFlt(0.0,1000.0,s);
         break;
      case 'd':
         if (NextArg()!=STRINGARG)
            HError(2319,"HERest: HMM definition directory expected");
         hmmDir = GetStrArg(); break;   
      case 'm':
         minEgs = GetChkedInt(0,1000,s); break;
      case 'o':
         if (NextArg()!=STRINGARG)
            HError(2319,"HERest: HMM file extension expected");
         newExt = GetStrArg(); break;
      case 'p':
         parMode = GetChkedInt(0,500,s); break;
      case 'r':
         twoDataFiles = TRUE; break;
      case 's':
         stats = TRUE;
         if (NextArg()!=STRINGARG)
            HError(2319,"HERest: Stats file name expected");
         statFN = GetStrArg(); break;
      case 't':
         pruneInit =  GetChkedFlt(0.0,1.0E20,s);
         if (NextArg()==FLOATARG || NextArg()==INTARG)
            {
               pruneInc = GetChkedFlt(0.0,1.0E20,s);
               pruneLim = GetChkedFlt(0.0,1.0E20,s);
            }
         else
            {
               pruneInc = 0.0;
               pruneLim = pruneInit  ;
            }
         break;
      case 'u':
         SetuFlags(); break;
      case 'v':
         minVar = GetChkedFlt(0.0,10.0,s); break;
      case 'w':
         mixWeightFloor = MINMIX * GetChkedFlt(0.0,10000.0,s); 
         break;
      case 'x':
         if (NextArg()!=STRINGARG)
            HError(2319,"HERest: HMM file extension expected");
         hmmExt = GetStrArg(); break;
      case 'B':
         saveBinary=TRUE;
         break;
      case 'F':
         if (NextArg() != STRINGARG)
            HError(2319,"HERest: Data File format expected");
         if((dff = Str2Format(GetStrArg())) == ALIEN)
            HError(-2389,"HERest: Warning ALIEN Data file format set");
         break;
      case 'G':
         if (NextArg() != STRINGARG)
            HError(2319,"HERest: Label File format expected");
         if((lff = Str2Format(GetStrArg())) == ALIEN)
            HError(-2389,"HERest: Warning ALIEN Label file format set");
         break;
      case 'H':
         if (NextArg() != STRINGARG)
            HError(2319,"HERest: HMM macro file name expected");
         AddMMF(&hset,GetStrArg());
         break;     
      case 'I':
         if (NextArg() != STRINGARG)
            HError(2319,"HERest: MLF file name expected");
         LoadMasterFile(GetStrArg());
         break;
      case 'L':
         if (NextArg()!=STRINGARG)
            HError(2319,"HERest: Label file directory expected");
         labDir = GetStrArg(); break;
      case 'M':
         if (NextArg()!=STRINGARG)
            HError(2319,"HERest: Output macro file directory expected");
         newDir = GetStrArg();
         break;     
      case 'T':
         trace = GetChkedInt(0,0100000,s);
         break;
      case 'X':
         if (NextArg()!=STRINGARG)
            HError(2319,"HERest: Label file extension expected");
         labExt = GetStrArg(); break;
      default:
         HError(2319,"HERest: Unknown switch %s",s);
      }
   } 
   if (NextArg() != STRINGARG)
      HError(2319,"HERest: file name of vocabulary list expected");

   Initialise(fbInfo, &fbInfoStack, &hset, GetStrArg());
   InitUttInfo(utt, twoDataFiles);
   numUtt = 1;

   if (trace&T_TOP) traceHFB=TRUE; /* allows HFB to do top-level tracing */

   do {
      if (NextArg()!=STRINGARG)
         HError(2319,"HERest: data file name expected");
      if (twoDataFiles && (parMode!=0)){
         if ((NumArgs() % 2) != 0)
            HError(2319,"HERest: Must be even num of training files for single pass training");
         strcpy(datafn1,GetStrArg());
         datafn = datafn1;
         
         datafn2 = GetStrArg();
      }else
         datafn = GetStrArg();
      if (parMode==0){
         src=LoadAccs(&hset, datafn);
         ReadFloat(&src,&tmpFlt,1,ldBinary);
         totalPr += (LogDouble)tmpFlt;
         ReadInt(&src,&tmpInt,1,ldBinary);
         totalT += tmpInt;
         CloseSource( &src );
      }
      else {
         DoForwardBackward(fbInfo, utt, datafn, datafn2) ;
         numUtt += 1;
      }
   } while (NumArgs()>0);
   if (parMode>0){
      MakeFN("HER$.acc",newDir,NULL,newFn);
      f=DumpAccs(&hset,newFn,parMode);
      tmpFlt = (float)totalPr;
      WriteFloat(f,&tmpFlt,1,ldBinary);
      WriteInt(f,(int*)&totalT,1,ldBinary);
      fclose( f );
   }else {
      if (stats) {
         StatReport(&hset);
      }
      UpdateModels(&hset,utt->pbuf2);
   }
   ResetHeap(&uttStack);
   ResetHeap(&fbInfoStack);
   ResetHeap(&hmmStack);
   Exit(0);
   return (0);          /* never reached -- make compiler happy */
}

/* -------------------------- Initialisation ----------------------- */

void Initialise(FBInfo *fbInfo, MemHeap *x, HMMSet *hset, char *hmmListFn)
{   
   HSetKind hsKind;
   int L,P,S,vSize,maxM; 

   /* Load HMMs and init HMMSet related global variables */
   if(MakeHMMSet( hset, hmmListFn )<SUCCESS)
      HError(2399,"Initialise: MakeHMMSet failed");
   if(LoadHMMSet( hset,hmmDir,hmmExt)<SUCCESS)
      HError(2399,"Initialise: LoadHMMSet failed");
   AttachAccs(hset, &gstack);
   ZeroAccs(hset);
   P = hset->numPhyHMM;
   L = hset->numLogHMM;
   vSize = hset->vecSize;
   S = hset->swidth[0];
   maxM = MaxMixInSet(hset);

   hsKind = hset->hsKind;
   if (hsKind==DISCRETEHS)
     uFlags = (UPDSet) (uFlags & (~(UPMEANS|UPVARS)));

   ConvDiagC(hset,TRUE);
   if (trace&T_TOP) {
      printf("HERest  Updating: ");
      if (uFlags&UPTRANS) printf("Transitions "); 
      if (uFlags&UPMEANS) printf("Means "); 
      if (uFlags&UPVARS)  printf("Variances "); 
      if (uFlags&UPMIXES && maxM>1)  printf("MixWeights "); 
      printf("\n\n ");
  
      if (parMode>=0) printf("Parallel-Mode[%d] ",parMode);
      if (pruneInit < NOPRUNE) 
         if (pruneInc != 0.0)
            printf("Pruning-On[%.1f %.1f %.1f]\n",pruneInit,pruneInc,pruneLim);
         else
            printf("Pruning-On[%.1f]\n",pruneInit);
      else
         printf("Pruning-Off");
      printf("System is ");
      switch (hsKind){
      case PLAINHS:  printf("PLAIN\n");  break;
      case SHAREDHS: printf("SHARED\n"); break;
      case TIEDHS:   printf("TIED\n"); break;
      case DISCRETEHS: printf("DISCRETE\n"); break;
      }
      printf("%d Logical/%d Physical Models Loaded, VecSize=%d\n",L,P,vSize);
      if (hset->numFiles>0)
         printf("%d MMF input files\n",hset->numFiles);
      if (mmfFn != NULL)
         printf("Output to MMF file:  %s\n",mmfFn); 
      fflush(stdout);
   }
   SetVFloor( hset, vFloor, minVar);
   totalPr = 0.0;

   /* initialise and  pass information to the forward backward library */
   InitialiseForBack(fbInfo, x, hset, NULL, uFlags, pruneInit, pruneInc,
                     pruneLim, minFrwdP);
}

/* ------------------- Statistics Reporting  -------------------- */

/* PrintStats: for given hmm */
void PrintStats(HMMSet *hset,FILE *f, int n, HLink hmm, int numEgs)
{
   WtAcc *wa;
   char buf[MAXSTRLEN];
   StateInfo *si;
   int i,N;
    
   N = hmm->numStates;
   ReWriteString(HMMPhysName(hset,hmm),buf,DBL_QUOTE);
   fprintf(f,"%4d %14s %4d ",n,buf,numEgs);
   for (i=2;i<N;i++) {
      si = hmm->svec[i].info;
      wa = (WtAcc *)((si->pdf+1)->hook);
      fprintf(f," %10f",wa->occ);
   }
   fprintf(f,"\n");
}

/* StatReport: print statistics report */
void StatReport(HMMSet *hset)
{
   HMMScanState hss;
   HLink hmm;
   FILE *f;
   int px;

   if ((f = fopen(statFN,"w")) == NULL){
      HError(2311,"StatReport: Unable to open stats file %s",statFN);
      return;
   }
   NewHMMScan(hset,&hss);
   px=1;
   do {
      hmm = hss.hmm;
      PrintStats(hset,f,px,hmm,(int)hmm->hook);
      px++;
   } while (GoNextHMM(&hss));
   EndHMMScan(&hss);
   fclose(f);
}

/* -------------------- Top Level of F-B Updating ---------------- */


/* Load data and call FBFile: apply forward-backward to given utterance */
void DoForwardBackward(FBInfo *fbInfo, UttInfo *utt, char * datafn, char * datafn2)
{
   utt->twoDataFiles = twoDataFiles ;
   utt->S = fbInfo->hset->swidth[0];

   /* Load the labels */
   LoadLabs(utt, lff, datafn, labDir, labExt);
   /* Load the data */
   LoadData(fbInfo->hset, utt, dff, datafn, datafn2);

   if (firstTime) {
      InitUttObservations(utt, fbInfo->hset, datafn, fbInfo->maxMixInS);
      firstTime = FALSE;
   }
  
   /* fill the alpha beta and otprobs (held in fbInfo) */
   FBFile(fbInfo, utt, datafn);

   /* update totals */
   totalT += utt->T ;
   totalPr += utt->pr ;

}

/* --------------------------- Model Update --------------------- */

static int nFloorVar = 0;     /* # of floored variance comps */
static int nFloorVarMix = 0;  /* # of mix comps with floored vars */

/* UpdateTrans: use acc values to calc new estimate for transP */
void UpdateTrans(HMMSet *hset, int px, HLink hmm)
{
   int i,j,N;
   float x,occi;
   TrAcc *ta;
   
   ta = (TrAcc *) GetHook(hmm->transP);
   if (ta==NULL) return;   /* already done */
   N = hmm->numStates;
   for (i=1;i<N;i++) {
      occi = ta->occ[i];
      if (occi > 0.0) 
         for (j=2;j<=N;j++) {
            x = ta->tran[i][j]/occi;
            hmm->transP[i][j] = (x>MINLARG)?log(x):LZERO;
         }
      else
         HError(-2326,"UpdateTrans: Model %d[%s]: no transitions out of state %d",
                px,HMMPhysName(hset,hmm),i);
   }
   SetHook(hmm->transP,NULL);
}

/* FloorMixes: apply floor to given mix set */
void FloorMixes(MixtureElem *mixes, int M, float floor)
{
   float sum,fsum,scale;
   MixtureElem *me;
   int m;
   
   sum = fsum = 0.0;
   for (m=1,me=mixes; m<=M; m++,me++) {
      if (me->weight>floor)
         sum += me->weight;
      else {
         fsum += floor; me->weight = floor;
      }
   }
   if (fsum>1.0) HError(2327,"FloorMixes: Floor sum too large");
   if (fsum == 0.0) return;
   if (sum == 0.0) HError(2328,"FloorMixes: No mixture weights above floor");
   scale = (1.0-fsum)/sum;
   for (m=1,me=mixes; m<=M; m++,me++)
      if (me->weight>floor) me->weight *= scale;
}

/* FloorTMMixes: apply floor to given tied mix set */
void FloorTMMixes(Vector mixes, int M, float floor)
{
   float sum,fsum,scale,fltWt;
   int m;
   
   sum = fsum = 0.0;
   for (m=1; m<=M; m++) {
      fltWt = mixes[m];
      if (fltWt>floor)
         sum += fltWt;
      else {
         fsum += floor;
         mixes[m] = floor;
      }
   }
   if (fsum>1.0) HError(2327,"FloorTMMixes: Floor sum too large");
   if (fsum == 0.0) return;
   if (sum == 0.0) HError(2328,"FloorTMMixes: No mixture weights above floor");
   scale = (1.0-fsum)/sum;
   for (m=1; m<=M; m++){
      fltWt = mixes[m];
      if (fltWt>floor)
         mixes[m] = fltWt*scale;
   }
}

/* FloorDProbs: apply floor to given discrete prob set */
void FloorDProbs(ShortVec mixes, int M, float floor)
{
   float sum,fsum,scale,fltWt;
   int m;
   
   sum = fsum = 0.0;
   for (m=1; m<=M; m++) {
      fltWt = Short2DProb(mixes[m]);
      if (fltWt>floor)
         sum += fltWt;
      else {
         fsum += floor;
         mixes[m] = DProb2Short(floor);
      }
   }
   if (fsum>1.0) HError(2327,"FloorDProbs: Floor sum too large");
   if (fsum == 0.0) return;
   if (sum == 0.0) HError(2328,"FloorDProbs: No probabilities above floor");
   scale = (1.0-fsum)/sum;
   for (m=1; m<=M; m++){
      fltWt = Short2DProb(mixes[m]);
      if (fltWt>floor)
         mixes[m] = DProb2Short(fltWt*scale);
   }
}

/* UpdateWeights: use acc values to calc new estimate of mix weights */
void UpdateWeights(HMMSet *hset, int px, HLink hmm)
{
   int i,s,m,M,N,S;
   float x,occi;
   WtAcc *wa;
   StateElem *se;
   StreamElem *ste;
   MixtureElem *me;
   HSetKind hsKind;

   N = hmm->numStates;
   se = hmm->svec+2;
   hsKind = hset->hsKind;
   S = hset->swidth[0];
   for (i=2; i<N; i++,se++){
      ste = se->info->pdf+1;
      for (s=1;s<=S; s++,ste++){
         wa = (WtAcc *)ste->hook;
         switch (hsKind){
         case TIEDHS:
            M=hset->tmRecs[s].nMix;
            break;
         case DISCRETEHS:
         case PLAINHS:
         case SHAREDHS:
            M=ste->nMix;
            break;
         }
         if (wa != NULL) {
            occi = wa->occ;
            if (occi>0) {
               for (m=1; m<=M; m++){
                  x = wa->c[m]/occi;
                  if (x>1.0){
                     if (x>1.001)
                        HError(2393,"UpdateWeights: Model %d[%s]: mix too big in %d.%d.%d %5.5f",
                               px,HMMPhysName(hset,hmm),i,s,m,x);
                     x = 1.0;
                  }
                  switch (hsKind){
                  case TIEDHS:
                     ste->spdf.tpdf[m] = (x>MINMIX) ? x : 0.0;
                     break;
                  case DISCRETEHS:
                     ste->spdf.dpdf[m]=(x>MINMIX) ? DProb2Short(x) : DLOGZERO;
                     break;
                  case PLAINHS:
                  case SHAREDHS:
                     me=ste->spdf.cpdf+m;
                     me->weight = (x>MINMIX) ? x : 0.0;
                     break;
                  }
               }
               if (mixWeightFloor>0.0){
                  switch (hsKind){
                  case DISCRETEHS:
                     FloorDProbs(ste->spdf.dpdf,M,mixWeightFloor);
                     break;
                  case TIEDHS:
                     FloorTMMixes(ste->spdf.tpdf,M,mixWeightFloor);
                     break;
                  case PLAINHS:
                  case SHAREDHS:
                     FloorMixes(ste->spdf.cpdf+1,M,mixWeightFloor);
                     break;
                  }
               }
            }else
               HError(-2330,"UpdateWeights: Model %d[%s]: no use of mixtures in %d.%d",
                      px,HMMPhysName(hset,hmm),i,s);
            ste->hook = NULL;
         }
      }
   }
}
      
/* UpdateMeans: use acc values to calc new estimate of means */
void UpdateMeans(HMMSet *hset, int px, HLink hmm)
{
   int i,s,m,k,M,N,S,vSize;
   float occim;
   MuAcc *ma;
   StateElem *se;
   StreamElem *ste;
   MixtureElem *me;
   Vector mean;
   
   N = hmm->numStates;
   se = hmm->svec+2;
   S = hset->swidth[0];
   for (i=2; i<N; i++,se++){
      ste = se->info->pdf+1;
      for (s=1;s<=S;s++,ste++){
         vSize = hset->swidth[s];
         me = ste->spdf.cpdf + 1; M = ste->nMix;
         for (m=1;m<=M;m++,me++)
            if (me->weight > MINMIX){
               mean = me->mpdf->mean;
               ma = (MuAcc *) GetHook(mean);
               if (ma != NULL){
                  occim = ma->occ;
                  if (occim > 0.0)
                     for (k=1; k<=vSize; k++)
                        mean[k] += ma->mu[k]/occim;
                  else
                     HError(-2330,"UpdateMeans: Model %d[%s]: no use of mean %d.%d.%d",
                            px,HMMPhysName(hset,hmm),i,s,m);
                  SetHook(mean,NULL);
               }
            }
      }
   }
}

/* UpdateTMMeans: use acc values to calc new estimate of means for TIEDHS */
void UpdateTMMeans(HMMSet *hset)
{
   int s,m,k,M,S,vSize;
   float occim;
   MuAcc *ma;
   MixPDF *mpdf;
   Vector mean;
   
   S = hset->swidth[0];
   for (s=1;s<=S;s++){
      vSize = hset->swidth[s];
      M = hset->tmRecs[s].nMix;
      for (m=1;m<=M;m++){
         mpdf = hset->tmRecs[s].mixes[m];
         mean = mpdf->mean;
         ma = (MuAcc *) GetHook(mean);
         if (ma != NULL){
            occim = ma->occ;
            if (occim > 0.0)
               for (k=1; k<=vSize; k++)
                  mean[k] += ma->mu[k]/occim;
            else
               HError(-2330,"UpdateMeans: No use of mean %d in stream %d",m,s);
            SetHook(mean,NULL);
         }
      }
   }
}

/* UpdateVars: use acc values to calc new estimate of variances */
void UpdateVars(HMMSet *hset, int px, HLink hmm)
{
   int i,s,m,k,l,M,N,S,vSize;
   float occim,x,muDiffk,muDiffl;
   Vector minV;
   VaAcc *va;
   MuAcc *ma;
   StateElem *se;
   StreamElem *ste;
   MixtureElem *me;
   Vector mean;
   Covariance cov;
   Boolean mixFloored,shared;
   
   N = hmm->numStates;
   se = hmm->svec+2;
   S = hset->swidth[0];
   for (i=2; i<N; i++,se++){
      ste = se->info->pdf+1;
      for (s=1;s<=S;s++,ste++){
         vSize = hset->swidth[s];
         minV = vFloor[s];
         me = ste->spdf.cpdf + 1; M = ste->nMix;
         for (m=1;m<=M;m++,me++)
            if (me->weight > MINMIX){
               cov = me->mpdf->cov;
               va = (VaAcc *) GetHook(cov.var);
               mean = me->mpdf->mean;
               ma = (MuAcc *) GetHook(mean);
               if (va != NULL){
                  occim = va->occ;
                  mixFloored = FALSE;
                  if (occim > 0.0){
                     shared=(GetUse(cov.var)>1 || ma==NULL || ma->occ<=0.0);
                     if (me->mpdf->ckind==DIAGC) {
                        for (k=1; k<=vSize; k++){
                           muDiffk=(shared)?0.0:ma->mu[k]/ma->occ;
                           x = va->cov.var[k]/occim - muDiffk*muDiffk;
                           if (x<minV[k]) {
                              x = minV[k];
                              nFloorVar++;
                              mixFloored = TRUE;
                           }
                           cov.var[k] = x;
                        }
                     }
                     else { /* FULLC */
                        for (k=1; k<=vSize; k++){
                           muDiffk=(shared)?0.0:ma->mu[k]/ma->occ;
                           for (l=1; l<=k; l++){
                              muDiffl=(shared)?0.0:ma->mu[l]/ma->occ;
                              x = va->cov.inv[k][l]/occim - muDiffk*muDiffl; 
                              if (k==l && x<minV[k]) {
                                 x = minV[k];
                                 nFloorVar++;
                                 mixFloored = TRUE;
                              }
                              cov.inv[k][l] = x;
                           }
                        }
                        CovInvert(cov.inv,cov.inv);
                     }
                  }
                  else
                     HError(-2330,"UpdateVars: Model %d[%s]: no use of variance %d.%d.%d",
                            px,HMMPhysName(hset,hmm),i,s,m);
                  if (mixFloored == TRUE) nFloorVarMix++;
                  SetHook(cov.var,NULL);
               }
            }
      }
   }
}

/* UpdateTMVars: use acc values to calc new estimate of vars for TIEDHS */
void UpdateTMVars(HMMSet *hset)
{
   int s,m,k,l,M,S,vSize;
   float occim,x,muDiffk,muDiffl;
   Vector minV;
   VaAcc *va;
   MuAcc *ma;
   MixPDF *mpdf;
   Vector mean;
   Covariance cov;
   Boolean mixFloored,shared;
   
   S = hset->swidth[0];
   for (s=1;s<=S;s++){
      vSize = hset->swidth[s];
      minV = vFloor[s];
      M = hset->tmRecs[s].nMix;
      for (m=1;m<=M;m++){
         mpdf = hset->tmRecs[s].mixes[m];
         cov = mpdf->cov;
         va = (VaAcc *) GetHook(cov.var);
         mean = mpdf->mean;
         ma = (MuAcc *) GetHook(mean);
         if (va != NULL){
            occim = va->occ;
            mixFloored = FALSE;
            if (occim > 0.0){
               shared=(GetUse(cov.var)>1 || ma==NULL || ma->occ<=0.0);
               if (mpdf->ckind==DIAGC) {
                  for (k=1; k<=vSize; k++){
                     muDiffk=(shared)?0.0:ma->mu[k]/ma->occ;
                     x = va->cov.var[k]/occim - muDiffk*muDiffk;
                     if (x<minV[k]) {
                        x = minV[k];
                        nFloorVar++;
                        mixFloored = TRUE;
                     }
                     cov.var[k] = x;
                  }
               }
               else { /* FULLC */
                  for (k=1; k<=vSize; k++){
                     muDiffk=(shared)?0.0:ma->mu[k]/ma->occ;
                     for (l=1; l<=k; l++){
                        muDiffl=(shared)?0.0:ma->mu[l]/ma->occ;
                        x = va->cov.inv[k][l]/occim - muDiffk*muDiffl;
                        if (k==l && x<minV[k]) {
                           x = minV[k];
                           nFloorVar++;
                           mixFloored = TRUE;
                        }
                        cov.inv[k][l] = x;
                     }
                  }
                  CovInvert(cov.inv,cov.inv);
               }
            }
            else
               HError(-2330,"UpdateTMVars: No use of var %d in stream %d",m,s);
            if (mixFloored == TRUE) nFloorVarMix++;
            SetHook(cov.var,NULL);
         }
      }
   }
}


/* UpdateModels: update all models and save them in newDir if set,
   new files have newExt if set */
void UpdateModels(HMMSet *hset, ParmBuf pbuf2)
{
   HSetKind hsKind;
   HMMScanState hss;
   HLink hmm;
   int px,n,maxM;
   static char str[100];
   BufferInfo info2;

   if (trace&T_UPD){
      printf("Starting Model Update\n"); fflush(stdout);
   }
   ConvDiagC(hset,TRUE); /* invert the variances again */
   hsKind = hset->hsKind;
   maxM = MaxMixInSet(hset);

   if (hsKind == TIEDHS){ /* TIEDHS - update mu & var once per HMMSet */
      if (uFlags & UPVARS)
         UpdateTMVars(hset);
      if (uFlags & UPMEANS)
         UpdateTMMeans(hset);
      if (uFlags & (UPMEANS|UPVARS))
         FixAllGConsts(hset);
   }

   NewHMMScan(hset,&hss);
   px=1;
   do {   
      hmm = hss.hmm;
      n = (int)hmm->hook;
      if (n<minEgs && !(trace&T_UPD))
         HError(-2331,"UpdateModels: %s[%d] copied: only %d egs\n",
                HMMPhysName(hset,hmm),px,n);
      if (trace&T_UPD) {
         if (n<minEgs)
            printf("Model %s[%d] copied: only %d examples\n",
                   HMMPhysName(hset,hmm),px,n);
         else
            printf("Model %s[%d] to be updated with %d examples\n",
                   HMMPhysName(hset,hmm),px,n);
         fflush(stdout);
      }
      if (n>=minEgs && n>0) {
         if (uFlags & UPTRANS)
            UpdateTrans(hset,px,hmm);
         if (maxM>1 && uFlags & UPMIXES)
            UpdateWeights(hset,px,hmm);
         if (hsKind != TIEDHS){
            if (uFlags & UPVARS)
               UpdateVars(hset,px,hmm);
            if (uFlags & UPMEANS)
               UpdateMeans(hset,px,hmm);
            if (uFlags & (UPMEANS|UPVARS))
               FixGConsts(hmm);
         }  
      }
      px++;
   } while (GoNextHMM(&hss));
   EndHMMScan(&hss);
   if (trace&T_TOP) {
      if (nFloorVar > 0)
         printf("Total %d floored variance elements in %d different mixes\n",
                nFloorVar,nFloorVarMix);
      fflush(stdout);
   }
   if (trace&T_TOP){
      if (mmfFn == NULL)
         printf("Saving hmm's to dir %s\n",(newDir==NULL)?"Current":newDir); 
      else
         printf("Saving hmm's to MMF %s\n",mmfFn);
      fflush(stdout);
   }
   ClearSeenFlags(hset,CLR_ALL);
   if (twoDataFiles){
      if (parMode == 0){
         SetChannel("HPARM2");
         nParm = GetConfig("HPARM2", TRUE, cParm, MAXGLOBS);
         GetConfStr(cParm,nParm,"TARGETKIND",str);
         hset->pkind = Str2ParmKind(str);
      }else{
         GetBufferInfo(pbuf2,&info2);
         hset->pkind = info2.tgtPK;
      }
   }
   SaveHMMSet(hset,newDir,newExt,saveBinary);
   if (trace&T_TOP)
      printf("Reestimation complete - average log prob per frame = %e\n",
             totalPr/totalT);
}

/* ----------------------------------------------------------- */
/*                      END:  HERest.c                         */
/* ----------------------------------------------------------- */
