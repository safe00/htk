#!/usr/local/bin/perl

# Global Variables
$trDataFiles="train.scr";    # Parameterised training data files
$teDataFiles="test.scr";    # Parameterised test data files
$dataFiles=$trDataFiles;             # Pointer to parameterised data files

$HHED="HHEd";
$HLED="HLEd";
$HCOPY="HCopy";
$HLIST="HList";
$HINIT="HInit";
$HREST="HRest";
$HEREST="HERest";
$HQUANT="HQuant";
$HSMOOTH="HSmooth";
$HRESULTS="HResults";
$HVITE="HVite";
$MAKEPROTO=".\\MakeProtoHMMSet";

$numIters=1;                                # Number of training iterations
$traceTools=0;                              # Display tool call tracing
$monLabDir="labels\\bcplabs\\mon";            # Dir for monophone label files
$triLabDir="labels\\bcplabs\\tri";            # Dir for triphone labels
$lbiLabDir="labels\\bcplabs\\lbi";            # Dir for left biphone labels
$rbiLabDir="labels\\bcplabs\\rbi";            # Dir for right biphone labels 
$labDir=$monLabDir;                         # Pointer to relevant labels dir
$monEdFile="edfiles\\edlabs.led";            # HLEd ed file to produce labels
$triEdFile="edfiles\\edtlabs.led";           # HLED ed file for tri labels
$lbiEdFile="edfiles\\edllabs.led";           # HLED ed file for lbi labels
$rbiEdFile="edfiles\\edrlabs.led";           # HLED ed file for rbi labels
$edFile=$monLabDir;                         # Pointer to relevant ed file
$contDepPlainhs="edfiles\\contDepPlainhs.hed"; # HHEd ed file for PLAINHS tris
$triSharedhsM1="edfiles\\triSharedhsM1.hed"; # HHEd ed file for SHAREDHS tris
$lbiSharedhsM1="edfiles\\lbiSharedhsM1.hed"; # HHEd ed file for SHAREDHS lbis
$rbiSharedhsM1="edfiles\\rbiSharedhsM1.hed"; # HHEd ed file for SHAREDHS rbis
$triTiedState="edfiles\\triTiedState.hed";   # HHEd ed file for tied state tris
$lbiTiedState="edfiles\\lbiTiedState.hed";   # HHEd ed file for tied state lbis
$rbiTiedState="edfiles\\rbiTiedState.hed";   # HHEd ed file for tied state rbis
$contDepTiedhsS1="edfiles\\contDepTiedhsS1.hed"; # HHEd ed file for TIEDHS S1
$contDepTiedhsS3="edfiles\\contDepTiedhsS3.hed"; # HHEd ed file for TIEDHS S3
$contDepTiedhs="";                        # Pointer to HHEd ed file for TIEDHS 
$hhedscript="";                      # Pointer to HHEd ed file for TIEDHS tris
$monTiedhsS1="edfiles\\monTiedhsS1.hed";     # HHEd ed file for TIEDHS mons S1
$monTiedhsS3="edfiles\\monTiedhsS3.hed";     # HHEd ed file for TIEDHS mons S3
$monTiedhs="";                      # Pointer to HHEd ed file for TIEDHS mons
$monSharedhsM1="edfiles\\monSharedhsM1.hed";# HHEd ed file for SHAREDHS mons M1
$monSharedhsM2="edfiles\\monSharedhsM2.hed";# HHEd ed file for SHAREDHS mons M2
$monHmmList="lists\\bcplist";                # List of monophones
$contDepHmmList="lists\\contDepList";        # List of 
$hmmList=$monHmmList;                       # Pointer to relevant HMM list
$monHmmVocab="lists\\bcpvocab";              # Vocab file for monophones
$triHmmVocab="lists\\triVocab";              # Vocab file for triphones
$lbiHmmVocab="lists\\lbiVocab";              # Vocab file for left biphones
$rbiHmmVocab="lists\\rbiVocab";              # Vocab file for right biphones
$hmmVocab=$monHmmVocab;                     # Pointer to relevant HMM vocab
$srcFmt="";                                 # Source file format
$triConv=0;                                 # Convert to triphones?
$configParams;                              # Global store of config parameters
$hrestConf="toolconfs\\hrest.conf";          # HRest conf file
$hinitConf="toolconfs\\hinit.conf";          # HInit conf file
$hcopyConf="toolconfs\\hcopy.conf";          # HCopy conf file
$hhedConf="toolconfs\\hhed.conf";            # HHEd conf file
$herestConf="toolconfs\\herest.conf";        # HERest conf file
$hsmoothConf="toolconfs\\hsmooth.conf";      # HSmooth conf file
$hviteConf="toolconfs\\hvite.conf";          # HVite conf file
$hquantConf="toolconfs\\hquant.conf";        # HQuant conf file
$macroStr="";               # String for tool -H option, either "-H file" or ""
$monNetwork="networks\\monLattice";          # Monophone network
$triNetwork="networks\\triLattice";          # Triphone network
$lbiNetwork="networks\\lbiLattice";          # Left biphone network
$rbiNetwork="networks\\rbiLattice";          # Right biphone network
$network=$monNetwork;                       # Pointer to recognition network
$cOptStr="";
$pOptStr="";
$aOptStr="";
$contDep="";

#***************************** START MAIN ***********************************
$|=1; #Force buffer flush on STDOUT

$NT_dir = `cd`;
$NT_dir =~ tr/a-z/A-Z/;
chomp($NT_dir);

$dir_pos = index($NT_dir, "HTKDEMO");
$get_dir = substr($NT_dir, $dir_pos, 7);


($get_dir =~ "HTKDEMO") || die "Must be in directory HTKDemo to run this script\n";
($#ARGV == 0) || die "USAGE: runDemo DemoConfigFile\n";

&ReadTCF();
$nS=$configParams{"nStreams"};
$cK=$configParams{"covKind"};

&SetToolConfs();

$numIters=$configParams{"HERest_Iter"};

if($configParams{"Clean_up"} =~ /^[yY]/){
    &CleanUp();
}

if($configParams{"Trace_tool_calls"} =~ /^[yY]/){
   $aOptStr="-A";
}

#*********************** PRE TRAINING ************************

if($configParams{"HCopy"} =~ /^[yY]/){
   &HCopy("data\\train", "lists\\trainFileStubs");
   &HCopy("data\\test", "lists\\testFileStubs");
}
if($configParams{"HList"} =~ /^[yY]/){
   &HList();
}
if($configParams{"HQuant"} =~ /^[yY]/){
   &HQuant();
}
if($nS==1){
    $contDepTiedhs = $contDepTiedhsS1;
    $monTiedhs = $monTiedhsS1;
}else{
    $contDepTiedhs = $contDepTiedhsS3;
    $monTiedhs = $monTiedhsS3;
}

#*************************** TRAINING ************************
if($configParams{"Context"} =~ /^[tTlLrR]/){
    print "Has the appropriate monophone system been generated Y/N?:";
    chop($ans = <STDIN>);
    if ($ans =~ /^[yY]/){
    }else{
	$hmmList=$monHmmList;
	&MkStdMonSys();
    }
    if($configParams{"Context"} =~ /^[tT]/){
	$contDep="tri";
	$network=$triNetwork;
	$hmmVocab=$triHmmVocab;
	$labDir=$triLabDir;
	$edFile=$triEdFile;
    }elsif($configParams{"Context"} =~ /^[lL]/){
	$contDep="lbi";
	$network=$lbiNetwork;
	$hmmVocab=$lbiHmmVocab;
	$labDir=$lbiLabDir;
	$edFile=$lbiEdFile;
    }elsif($configParams{"Context"} =~ /^[rR]/){
	$contDep="rbi";
	$network=$rbiNetwork;
	$hmmVocab=$rbiHmmVocab;
	$labDir=$rbiLabDir;
	$edFile=$rbiEdFile;
    }
    $hmmList=$contDepHmmList;
    &MkContDepSys();
}else{
    $network=$monNetwork;
    $hmmList=$monHmmList;
    $hmmVocab=$monHmmVocab;
    &MkMonSys();
}

#*************************** TESTING ************************
if($configParams{"hsKind"} =~ /^[pPsS]/){
    $pOptStr="-p 5.0";
}else{
    $pOptStr="-p -1.0";
}
if($configParams{"direct_audio"} =~ /^[yY]/){
    &HViteAudio($hmmVocab,$hmmList,$network);
}elsif($configParams{"HVite"} =~ /^[yY]/){
    print "\nTesting on the training set\n";
    $dataFiles=$trDataFiles;
    &HVite($network,$labDir,$hmmVocab,$hmmList,$dataFiles);
    system("del /Q test\\*");
    print "\nTesting on the test set\n";
    $dataFiles=$teDataFiles;
    &HVite($network,$labDir,$hmmVocab,$hmmList,$dataFiles);
}

#******************************* END MAIN *********************************

#************************ Util Functions **********************************

#----------------------------------------------
# MkContDepSys: make a context dependent system
#----------------------------------------------
sub MkContDepSys {
    local($i,$mmix)=0;

    unless($configParams{"covKind"} =~ /^[dD]/){
	die "Can't make context dependent from full covariance monophones\n";
    }
    if ($configParams{"hsKind"} =~ /^[pPsS]/){
	for ($i=1; $i<=$nS; $i++){
	    if ($mixes[$i] > 1){
		$mmix=1;
	    }
	}
    }
    unless($mmix == 0){
	die "Can't make context dependent from multiple mixture monophones\n";
    }
    print ("WARNING: Monophones should have no parameter sharing\n");
    $srcDir="hmms\\hmm.2";
    $tgtDir="hmms\\hmm.1";
    #$labFiles="$monLabDir\\*";
    $labFiles="monlabs.scr";
    &HLEd($srcFmt,$labDir,$hmmList,$edFile,$labFiles);

    if($configParams{"hsKind"} =~ /^[sS]/){
	&HHEd($srcDir,$tgtDir,$monHmmList,$contDepPlainhs);
	system("del /Q hmms\\hmm.2\\*");
	print "\nTraining the plain triphone system created\n";
	&HERest($numIters,$labDir,$hmmList);
	if($configParams{"TiedState"} =~ /^[yY]/){
	    $hhedscript="${contDep}TiedState";
	    &HHEd($srcDir,$tgtDir,$hmmList,$$hhedscript);
	}else{
	    $hhedscript="${contDep}SharedhsM1";
	    &HHEd($srcDir,$tgtDir,$hmmList,$$hhedscript);
	}
    }elsif($configParams{"hsKind"} =~ /^[pPdD]/){
	&HHEd($srcDir,$tgtDir,$monHmmList,$contDepPlainhs);
    }elsif($configParams{"hsKind"} =~ /^[tT]/){
	&HHEd($srcDir,$tgtDir,$monHmmList,$contDepPlainhs);
	system("del /Q hmms\\hmm.2\\*");
	print "\nTraining the plain triphone system created\n";
	&HERest($numIters,$labDir,$hmmList);
	&HHEd($srcDir,$tgtDir,$hmmList,$contDepTiedhs);
	$cOptStr="-c 50.0";
    }  
    if($configParams{"HERest"} =~ /^[yY]/){
	if($configParams{"HERest_par_mode"} =~ /^[yY]/){
	    &HERestPar($numIters,$labDir,$hmmList);
	}else{
	    &HERest($numIters,$labDir,$hmmList);
	}
    }elsif($configParams{"HSmooth"} =~ /^[yY]/){
	&HSmooth($numIters,$labDir,$hmmList);
    }
}
sub AddMixNums {
    local($i);

    for ($i=1; $i<=$nS; $i++){
	if($configParams{"hsKind"} =~ /^[tT]/){
	    $protoName=$protoName."1";
	    if( $i != $nS ){
		$protoName=$protoName."_";
	    }
	}else{
	    $protoName=$protoName.$mixes[$i];
	    if( $i == $nS ){
		chop( $protoName );
	    }else{
		$protoName=$protoName."_";
	    }
	}
    }
}

#----------------------------------
# MkMonSys: make a monophone system
#----------------------------------
sub MkMonSys {
    local($fromProto)=0;

    $protoName="proto_s${nS}_m";
    &AddMixNums();
    if($configParams{"hsKind"} =~ /^[dD]/){
	$protoName=$protoName."_vq.pcf";
    }else{
	$protoName=$protoName."_${cK}c.pcf";
    }
(-f "protoconfs\\$protoName")||die "Cannot find proto config file protoconfs\\$protoName\n";

    #$labFiles="tidata\\*.phn";
    $labFiles="tidata.scr";
    $srcFmt="-G TIMIT";

    if($configParams{"HLEd"} =~ /^[yY]/){
	&HLEd($srcFmt,$labDir,$hmmList,$edFile,$labFiles);
    }
    if($configParams{"HInit"} =~ /^[yY]/){
	&HInit($labDir,$protoName,$hmmList);
    }
    if($configParams{"HRest"} =~ /^[yY]/){
	&HRest($labDir,$hmmList);
    }
    if($configParams{"HERest"} =~ /^[yY]/){
	if($configParams{"HERest_par_mode"} =~ /^[yY]/){
	    &HERestPar($numIters,$labDir,$hmmList);
	}else{
	    &HERest($numIters,$labDir,$hmmList);
	}
    }
    $srcDir="hmms\\hmm.1";
    $tgtDir="hmms\\hmm.0";
    if($configParams{"hsKind"} =~ /^[sS]/){
	&HHEd($srcDir,$tgtDir,$hmmList,$monSharedhsM1);
	&HRest($monLabDir,$hmmList);
	&HERest($numIters,$labDir,$hmmList);
    }elsif($configParams{"hsKind"} =~ /^[tT]/){
	&HHEd($srcDir,$tgtDir,$hmmList,$monTiedhs);
	&HRest($monLabDir,$hmmList);
	&HERest($numIters,$labDir,$hmmList);
    }
}

#----------------------------------------------
# MkStdMonSys: make a standard monophone system
#----------------------------------------------
sub MkStdMonSys {
    #$labFiles="tidata\\*.phn";
    $labFiles="tidata.scr";
    $srcFmt="-G TIMIT";

    $protoName="proto_s${nS}_m";
    if($configParams{"hsKind"} =~ /^[dD]/){
	&AddMixNums();
	$tmp=$protoName."_vq.pcf";
	$protoName = $tmp;
    }else{
	$protoName="proto_s${nS}_m1_dc.pcf";
    }
(-f "protoconfs\\$protoName")||die "Cannot find proto config file protoconfs\\$protoName\n";

    if($configParams{"HLEd"} =~ /^[yY]/){
	&HLEd($srcFmt,$labDir,$hmmList,$edFile,$labFiles);
    }
    
    &CleanUp();

    &HInit($monLabDir,$protoName,$hmmList);
    &HRest($monLabDir,$hmmList);
    &HERest($numIters,$labDir,$hmmList);
}

#------------------------------------------
# CleanUp: Clear model and file directories
#------------------------------------------
sub CleanUp {
    print "Cleaning up directories\n";
    system("del /Q hmms\\hmm.0\\*");
    system("del /Q hmms\\hmm.1\\*");
    system("del /Q hmms\\hmm.2\\*");
    system("del /Q hmms\\tmp\\*");
    system("del /Q test\\*");
    system("del /Q accs\\hmm.2\\*");
    system("del /Q proto\\*");
    print "Done cleaning\n";
}

#-----------------------------------------------------------
# SetToolConfs: Set tool configuration files for discrete or
#               continuous density HMMs
#-----------------------------------------------------------
sub SetToolConfs {
    local($sysId);

    if($configParams{"hsKind"} =~ /^[dD]/){
	$sysId=VQ;
    }else{
	$sysId=CD;
    }
    system("copy toolconfs\\hinit${sysId}.conf toolconfs\\hinit.conf");
    system("copy toolconfs\\hrest${sysId}.conf toolconfs\\hrest.conf");
    system("copy toolconfs\\herest${sysId}.conf toolconfs\\herest.conf");
    system("copy toolconfs\\hsmooth${sysId}.conf toolconfs\\hsmooth.conf");
    system("copy toolconfs\\hvite${sysId}.conf toolconfs\\hvite.conf");
    if($configParams{"direct_audio"} =~ /^[yY]/){
	system("copy toolconfs\\hcopyDA.conf toolconfs\\hcopy.conf");
	system("copy toolconfs\\hviteDA.conf toolconfs\\hvite.conf");
    }else{
	system("copy toolconfs\\hcopyFB.conf toolconfs\\hcopy.conf");
    }
}


#--------------------------------------------------------------------
# ReadTCF: Reads the Test Config File and sets parameters accordingly 
#--------------------------------------------------------------------
sub ReadTCF {

local($validData,$param,$val)=0;

open(CONFIG, "configs/$a"); #read tcf from STDIN

while(<>){
    if(/\<ENDsys_setup\>/ || /\<ENDtool_steps\>/){
	$validData=0;
    }
    if($validData){
	($param,$val)=split(/ *: */, $_);
	if ($param =~ /nMixes/){
	    @mixes=split(/ +/, $_);
	}
	$val =~ tr\A-Z\a-z\;
	chop($val);
	$configParams{$param}=$val;
	write;
    }
    if(/\<BEGINsys_setup\>/ || /\<BEGINtool_steps\>/){
	$validData=1;
    }
}
format STDOUT_TOP =
 Test Config File Read
 =====================
Parameter         Value
-----------------------
.
format STDOUT=
@<<<<<<<<<<<<<<<<<<<@<<<<<<<<<<
$param,$val
.

}
#-------------------------------------------------------------------
# TestDirEmpty: Tests if directory is empty and prompts user for the 
#               deletion of any files found
#-------------------------------------------------------------------
sub TestDirEmpty {
    # Get arguments
    local($dirName, $srcOrTgt, $tool) = @_;
    local(@nFiles,$rtnVal);

    $rtnVal=1;
    opendir(DIR,$dirName) || die "Can't open $dirName\n";
    @nFiles = grep(!/^\./, readdir(DIR)); #Forget about . files
    if($nFiles[0]){
	if($srcOrTgt eq "tgt"){
	    print "\n$dirName not empty, overwrite using $tool Y/N?:";
	    chop($ans = <STDIN>);
	    if ($ans =~ /^[yY]/){
		print "\nRemoving files from $dirName\n";
		system("del /Q $dirName\\*");
		$rtnVal=1;
	    }else{
		print "\nDirectory $dirName unaltered, skipping to next test\n";
		$rtnVal=0;
	    }
	}
    }else{
	if($srcOrTgt eq "src"){
	    die "Source Directory Empty $dirName\n";
	}           
    }
    $rtnVal;
}

#-------------------------------------------------------------------
# SetMacroStr: Generate -H option string dependent on whether source
#              directory contains a macro file
#------------------------------------------------------------------- 
sub SetMacroStr {
    # Get arguments
    local($srcDir) = @_;

    if (-r $srcDir."\\newMacros"){
	$macroStr = "-H ".$srcDir."\\newMacros";
    }else{
	$macroStr = "";
    }
}  

#************************** HTK Tool Functions ********************************

#--------------------------------------------------
# HHEd: Calls HHEd to perform editing on the HMMSet
#--------------------------------------------------        
sub HHEd {
    # Get arguments
    local($srcDir,$tgtDir,$hmmList,$hedFile) = @_;

    &SetMacroStr($srcDir);

    &TestDirEmpty($srcDir,"src","HHEd - $hedFile");
    if (&TestDirEmpty($tgtDir,"tgt","HHEd - $hedFile")){
	system("$HHED $aOptStr -C $hhedConf -D -d $srcDir $macroStr -M $tgtDir $hedFile $hmmList");
    }
}

#-------------------------------------------------------------------------
# HCopy: Calls HCopy to convert TIMIT data files to HTK parameterised ones
#-------------------------------------------------------------------------
sub HCopy {
    local($tgtDir,$fileStubList)=@_;

    local($srcDir)="tidata";

    &TestDirEmpty($srcDir,"src","HCopy");
    if (&TestDirEmpty($tgtDir,"tgt","HCopy")){
	open(FILESTUBLIST, $fileStubList);
	while(<FILESTUBLIST>) {   # read HMM name into $_
	    chop($_);
	    system("$HCOPY $aOptStr -C $hcopyConf -D $srcDir\\$_.adc $tgtDir\\$_.mfc");
	}
    }
}

#------------------------------------------------------
# HQuant: Calls HQuant to produce a 64 code VQ codebook
#------------------------------------------------------
sub HQuant {
    local($srcDir)="data\\train";
    local($tgtDir)="codebooks";
    local($nOptStr,$tOptStr,$codebook,$lin);

    &TestDirEmpty($srcDir,"src","HQuant");
    if ($nS==1){
	$nOptStr="-s 1 -n 1 $mixes[1]";
	$codebook=$tgtDir."\\C${mixes[1]}";
    }else{
	$nOptStr="-s 3 -n 1 $mixes[1] -n 2 $mixes[2] -n 3 $mixes[3]";
	$codebook=$tgtDir."\\C${mixes[1]}_${mixes[2]}_${mixes[3]}";
    }
    if($configParams{"VQ_clust"} =~ /^[tT]/){
	$tOptStr="-t"; $lin=0;
    }else{
	$tOptStr=""; $lin=1;
    }
    chop($nOptStr);
    chop($codebook);
    $codebook=$codebook."cb";
    if (-f $codebook){
	print "$codebook already exists, overwrite using HQuant (Y/N)?:";
	chop($ans = <STDIN>);
    }else{
	$ans=Y;
    }
    if ($ans =~ /^[yY]/){
	print "\nConstructing a VQ codebook\n";
	if( $lin ){
	    print "WARNING - Go and get a coffee, this will take some time\n";
	}
	#system("$HQUANT -d $aOptStr -C $hquantConf -D $nOptStr $tOptStr -T 1 -S $codebook $srcDir\\tr*.mfc");
	system("$HQUANT -d $aOptStr -C $hquantConf -D $nOptStr $tOptStr -T 1 -S $trDataFiles $codebook");
    } 
    system("echo copying $codebook codebooks\\currentCodebook");
    system("copy $codebook codebooks\\currentCodebook");
}

#---------------------------------------------------------------
# HInit: Calls initialisation tool HInit for each HMM in hmmList
#---------------------------------------------------------------        
sub HInit {
    # Get arguments
    local($labDir,$protoName,$hmmList) = @_;
    local($tgtDir)="hmms\\hmm.0";
    local($srcDir)="proto";
    
    system("echo \"\n\nperl $MAKEPROTO protoconfs\\$protoName\"");
    system("perl $MAKEPROTO protoconfs\\$protoName");

    &SetMacroStr($srcDir);

    if (&TestDirEmpty($tgtDir,"tgt","HInit")){
	open(HMMLIST, $hmmList);
	while(<HMMLIST>) {   # read HMM name into $_
	    chop($_);
	    print(STDOUT "\n\nCalling HInit for HMM ",$_,"\n");
	    #system("($HINIT $aOptStr -i 10 -L $labDir -l $_ -o $_ $macroStr -C $hinitConf -D -M $tgtDir -T 1 -S $dataFiles $srcDir\$_)");
	    system("$HINIT $aOptStr -i 10 -L $labDir -l $_ -o $_ $macroStr -C $hinitConf -D -M $tgtDir -T 1 -S $dataFiles $srcDir/$_");
	}
	close(HMMLIST);
    }
}

#-----------------------------------------------------------------------
# HRest: Calls isolated re-estimation tool HRest for each HMM in hmmList
#-----------------------------------------------------------------------
sub HRest {
    # Get arguments
    local($labDir,$hmmList) = @_;
    local($srcDir,$tgtDir);

    $srcDir="hmms\\hmm.0";
    $tgtDir="hmms\\hmm.1";

    &SetMacroStr($srcDir);

    &TestDirEmpty($srcDir,"src","HRest");

    if (&TestDirEmpty($tgtDir,"tgt","HRest")){
	open(HMMLIST, $hmmList);
	while(<HMMLIST>) {   # read HMM name into $_
	    chop($_);
	    print(STDOUT "\n\nCalling HRest for HMM ",$_,"\n");
	    system("($HREST $aOptStr -u tmvw -w 3 -v 0.05 -i 10 -L $labDir -l $_ -C $hrestConf $macroStr -D -M $tgtDir -T 1 -S $dataFiles $srcDir\\$_)");
	}
	close(HMMLIST);
    }
}

#-------------------------------------------------------------------------
# HERest: Calls embedded re-estimation tool HERest on all HMMs in hmmList
#         for the required number of iterations
#-------------------------------------------------------------------------
sub HERest {
    # Get arguments
    local($numIters,$labDir,$hmmList) = @_;
    local($srcDir,$tgtDir,$tmpDir,$i);

    $srcDir="hmms\\hmm.1";
    $tgtDir="hmms\\hmm.2";
    $tmpDir="hmms\\tmp";

    &TestDirEmpty($srcDir,"src","HERest");

    &SetMacroStr($srcDir);
 
    if (&TestDirEmpty($tgtDir,"tgt","HERest")){
	$i=1;
	while($i<=$numIters){
	    print (STDOUT "\n\nIteration ",$i," of Embedded Re-estimation\n");
	    system("$HEREST $aOptStr -w 3 -v 0.05 -C $herestConf -u tmvw $cOptStr -d $srcDir $macroStr -D -M $tgtDir -L $labDir -t 2000.0 -T 1 -S $dataFiles $hmmList");
	    if ($numIters > 1){
		system("copy $tgtDir\\* $tmpDir");
		$srcDir=$tmpDir;
		&SetMacroStr($srcDir);
	    }
	    $i++;
	}
	system("del /Q $tmpDir\\*");
    }
}


#------------------------------------------------------------------
# HVite: Calls Viterbi recognition tool HVite using HMMs in hmmList
#        and test files in testSet and network
#------------------------------------------------------------------
sub HVite {
    # Get arguments
    local($net,$labDir,$hmmVocab,$hmmList,$testSet) = @_;
    local($srcDir,$tgtDir);

    $srcDir="hmms\\hmm.2";
    $tgtDir="test";

    &TestDirEmpty($srcDir,"src","HVite");

    $dataFiles=$testSet;

    &SetMacroStr($srcDir);
 
    if (-e "recout.mlf"){    
       system("del /Q recout.mlf");
    }

    
    if (&TestDirEmpty($tgtDir,"tgt","HVite")){
       system("$HVITE $aOptStr -C $hviteConf -d $srcDir -l test -i recout.mlf -w $net $macroStr -D -L $tgtDir -t 300.0 -T 1 $pOptStr -s 0.0 -S $dataFiles $hmmVocab $hmmList");
    }
    #system("$HRESULTS $aOptStr -s -L $labDir $hmmList test\\*.rec");
    system("$HRESULTS $aOptStr -s -L $labDir $hmmList recout.mlf");
}

#--------------------------------------------------------------
# HViteAudio: Calls Viterbi recognition tool HVite taking input
#             directly from the machine audio
#--------------------------------------------------------------
sub HViteAudio {
    # Get arguments
    local($hmmVocab,$hmmList,$net) = @_;
    local($srcDir,$tgtDir,$conf,$carryOn);

    $srcDir="hmms\\hmm.2";
    $tgtDir="test";
    $conf="toolconfs\\hvite.conf";
    $carryOn=1;

    &TestDirEmpty($srcDir,"src","HVite");

    if( `ps | grep HVite` ){
	die "ERROR: There is an HVite process running already\n";
    }else{
	print "*****************************************\n";
	print "*        At the READY[x]> prompt        *\n";
	print "*    Press rtn to start\stop speaking   *\n";
	print "* Press any key followed by rtn to exit *\n";
	print "*****************************************\n\n";
	system("($HVITE -A -T 1 -t 150.0 -g -n 3 5 -e -i outaudio -C $conf -d $srcDir -w $net -p -10.0 -s 5.0 $hmmVocab $hmmList)&");
	@pid=split(/ +/, `ps | grep HVite`);

	while ($carryOn) {
	    $ans = <STDIN>;
	    if ($ans =~ /^\n/){
		kill 16, $pid[1];
	    }else{
		kill 9, $pid[1];
		$carryOn=0;
	    }
	}
    }
}


#-------------------------------------------------------------------------
# HERestPar: Calls embedded re-estimation tool HERest in parallel mode on 
#            all HMMs in hmmList hmmList for the required number of 
#            iterations
#-------------------------------------------------------------------------
sub HERestPar {
    # Get arguments
    local($numIters,$labDir,$hmmList) = @_;
    local($srcDir,$tgtDir,$tmpDir,$i,$accOrDataFiles,$parNum);

    $srcDir="hmms\\hmm.1";
    $tgtDir="hmms\\hmm.2";
    $accDir="accs\\hmm.2";
    $tmpDir="hmms\\tmp";
    $parNum=1;

    $accOrDataFiles=$dataFiles;

    &TestDirEmpty($srcDir,"src","HERest - parallel");

    &SetMacroStr($srcDir);

    if (&TestDirEmpty($accDir,"tgt","HERest - parallel")){
	$i=1;
	while($i<=$numIters){
	    print (STDOUT "\n\nIteration ",$i," of Embedded Re-estimation\n");
	    system("$HEREST $aOptStr -C $herestConf -w 3 -v 0.05 -p $parNum -d $srcDir $macroStr -D -M $accDir -L $labDir -t 2000.0 -T 1 -S $accOrDataFiles $hmmList");
	    $parNum=0;
	    $accOrDataFiles=$accDir."\\HER1.acc";
	    system("$HEREST $aOptStr -C $herestConf -w 3 -v 0.05 -p $parNum -d $srcDir $macroStr -D -M $tgtDir -L $labDir -t 2000.0 -T 1 $hmmList $accOrDataFiles ");
	    if ($numIters > 1){
		print "\n\n";
		system("echo \"copy $tgtDir\\* $tmpDir\"");
		system("copy $tgtDir\\* $tmpDir\\");
		$srcDir=$tmpDir;
		&SetMacroStr($srcDir);
	    }
	    $i++;
	    $parNum=1;
	    $accOrDataFiles=$dataFiles;
	}
	system("del /Q $tmpDir\\*");
    }
}



#-------------------------------------------------------------------------
# HSmooth:   Calls embedded re-estimation tool HERest in parallel mode on 
#            all HMMs in hmmList followed by the deleted interpolation
#            tool HSmooth
#-------------------------------------------------------------------------
sub HSmooth {
    # Get arguments
    local($numIters,$labDir,$hmmList) = @_;
    local($srcDir,$tgtDir,$tmpDir,$i,$j,$dataList,$dataListStub);

    $srcDir="hmms\\hmm.1";
    $tgtDir="hmms\\hmm.2";
    $accDir="accs\\hmm.2";
    $tmpDir="hmms\\tmp";
    $dataListStub="lists\\dataList";

    $accOrDataFiles=$dataFiles;

    &TestDirEmpty($srcDir,"src","HSmooth");
    &SetMacroStr($srcDir);

    if (&TestDirEmpty($accDir,"tgt","HSmooth")){
	$i=1;
	while($i<=$numIters){
	    $j=1;
	    while($j<=3){
		$dataList=$dataListStub.$j;
		print (STDOUT "\n\nIteration ",$i," of Embedded Re-estimation\n");
		system("$HEREST $aOptStr -C $herestConf -w 3 -v 0.05 -p $j -d $srcDir $macroStr -D -M $accDir -L $labDir -t 2000.0 -T 1 -S $dataList $hmmList");
		$j++;   
	    }
	    $parNum=0;
	    $accOrDataFiles="accs.scr";
	    system("$HSMOOTH $aOptStr -C $hsmoothConf -d $srcDir $macroStr -D -M $tgtDir -T 1 -S accs.scr $hmmList");
	    if ($numIters > 1){
		print "\n\n";
		system("echo \"copy $tgtDir\\* $tmpDir\"");
		system("copy $tgtDir\\* $tmpDir");
		$srcDir=$tmpDir;
		&SetMacroStr($srcDir);
	    }
	    $i++;
	    $parNum=1;
	    $accOrDataFiles=$dataFiles;
	}
	system("del /Q $tmpDir\\*");
    }
}

#------------------------------------
# HLEd: Invokes the label editor HLEd
#------------------------------------
sub HLEd {
    # Get arguments
    local($srcFmt,$labDir,$hmmList,$ledFile,$labFiles) = @_;

    if (&TestDirEmpty($labDir,"tgt","HLEd")){
       system("$HLED $aOptStr $srcFmt -l $labDir -n $hmmList -D -T 1 -S $labFiles $ledFile");
    }
}

#----------------------------------------------------------------------------
# HList: Invokes HList to display the headers of all parameterised data files
#        created for training together with the 1st 10 lines of the 1st file.
#----------------------------------------------------------------------------
sub HList {
    local($tr1fname)="data\\train\\tr1.mfc";

    print (STDOUT "Display some of the data files\n");
    print (STDOUT "This demonstrates the use of HList\n");
    
    unless(-r $tr1fname){
	die "Cannot read file $tr1fname\n";
    }
    print (STDOUT"\n\nTraining data file headers\n");
    system("$HLIST $aOptStr -D -h -z -S $trDataFiles");
    print (STDOUT "\n\nFirst 10 frames of tr1.mfc (with deltas appended)\n");
    system("$HLIST -aOptStr -e 10 $tr1fname");
}
