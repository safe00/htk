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
/*         File: HFB.h: Forward Backward routines module       */
/* ----------------------------------------------------------- */

/* !HVER!HFB:   3.0 [CUED 05/09/00] */

#ifndef _HFB_H_
#define _HFB_H_

#ifdef __cplusplus
extern "C" {
#endif

#define NOPRUNE 1.0E20

enum _UPDSet{UPMEANS=1,UPVARS=2,UPTRANS=4,UPMIXES=8,UPADAPT=16};
typedef enum _UPDSet UPDSet;

/* structure for the utterance information */
typedef struct {

  MemHeap transStack; /* utterance transcript information heap */
  MemHeap dataStack;  /* utterance data information heap */
  MemHeap dataStack2; /* utterance data2 information heap */

  int Q;              /* number of models in transcription */
  Transcription *tr;  /* current transcription */

  Boolean twoDataFiles; /* Using two data files */
  int S;              /* number of data streams */
  int T;              /* number of frames in utterance */
  ParmBuf pbuf;       /* parameter buffer */
  ParmBuf pbuf2;      /* a second parameter buffer (if required) */

  Observation ot;      /* Observation at time t ... */
  Observation ot2;     /* Cepstral Mean Normalised obervation, used in
                               single pass re-training */

  LogDouble pr;        /* log prob of current utterance */

} UttInfo;


/* structure for the forward-backward  pruning information */
typedef struct {
 
  short *qHi;               /* array[1..T] of top of pruning beam */
  short *qLo;               /* array[1..T] of bottom of pruning beam */
  int maxBeamWidth;         /* max width of beam in model units */
  LogDouble maxAlphaBeta;   /* max alpha/beta product along beam ridge */
  LogDouble minAlphaBeta;   /* min alpha/beta product along beam ridge */
  LogDouble pruneThresh;    /* pruning threshold currently */

} PruneInfo;

/* structure for the forward-backward alpha-beta structures */
typedef struct {
  
  MemHeap abMem;      /* alpha beta memory heap */
  PruneInfo *pInfo;   /* pruning information */
  HLink *qList;       /* array[1..Q] of active HMM defs */
  LabId  *qIds;       /* array[1..Q] of logical HMM names (in qList) */
  short *qDms;        /* array[1..Q] of minimum model duration */
  DVector *alphat;    /* array[1..Q][1..Nq] of prob */
  DVector *alphat1;   /* alpha[t-1] */
  DVector **beta;     /* array[1..T][1..Q][1..Nq] of prob */
  float ****otprob;   /* array[1..T][1..Q][2..Nq-1][0..S] of prob */
  LogDouble pr;       /* log prob of current utterance */
  Vector occt;        /* occ probs for current time t */
  Vector *occa;       /* array[1..Q][1..Nq] of occ probs (trace only) */

} AlphaBeta;

/* structure storing the model set and a pointer to it's alpha-beta pass structure */
typedef struct {

  HMMSet *hset;       /* set of HMMs to be re-estimated */
  HSetKind hsKind;    /* kind of the HMM system */
  UPDSet uFlags;      /* parameter update flags */
  int skipstart;      /* Skipover region - debugging only */
  int skipend;
  int maxM;           /* maximum number of mixtures in hmmset */
  int maxMixInS[SMAX];/* array[1..swidth[0]] of max mixes */
  AlphaBeta *ab;      /* Alpha-beta structure for this model */
  RegTransInfo *rt;   /* Adaptation regression transform info 
			 only used if adaptation is being performed */
} FBInfo;


/* EXPORTED FUNCTIONS-------------------------------------------------*/

/* Initialise HFB module */
void InitFB(void) ;

/* Initialise the forward backward memory stacks etc */
void InitialiseForBack(FBInfo *fbInfo, MemHeap *x, HMMSet *set,
		       RegTransInfo *rt, UPDSet uset,
		       LogDouble pruneInit, LogDouble pruneInc, 
		       LogDouble pruneLim, float minFrwdP);

/* Initialise the utterance Information */
void InitUttInfo(UttInfo *utt, Boolean twoFiles );

/* GetInputObs: Get input Observations for t */
void GetInputObs( UttInfo *utt, int t, HSetKind hsKind );

/* load the labels into the UttInfo structure from file */
void LoadLabs(UttInfo *utt, FileFormat lff, char * datafn,
	      char *labDir, char *labExt);

/* load the data file(s) into the UttInfo structure */
void LoadData(HMMSet *hset, UttInfo *utt, FileFormat dff, 
	      char * datafn, char * datafn2);

/* Initialise the observation structures within UttInfo */
void InitUttObservations(UttInfo *utt, HMMSet *hset,
			 char * datafn, int * maxmixInS);

/* FBFile: apply forward-backward to given utterance */
void FBFile(FBInfo *fbInfo, UttInfo *utt, char * datafn);

/* PrLog: print a log value */
void PrLog(LogDouble x);

#ifdef __cplusplus
}
#endif

#endif  /* _HFB_H_ */
