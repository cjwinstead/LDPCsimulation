//==============================================================
// decodeGDBF.cpp
// 
// This program performs GDBF decoding on AWGN channel.
// Supports parallel flipping (multi-bit flipping) and
// sequential flipping (single-bit flipping), and also
// Wadayama's "mode-switching" technique which a phase
// of parallel flipping followed by a longer phase of
// sequential flipping.
//==============================================================

//--- COMPILE OPTIONS ---//
// Possible algorithm modes. Implement the different options
// by passing them to the compiler via -D <option>.
// Note that some options don't make sense together (e.g. 
// modeswitching and sequentialmode).
/*
//#define parallelmode
//#define sequentialmode  // Use sequential flipping only
//#define modeswitching   // Use Wadayama's parallel-serial mode-switching technique for improved performance
//#define addNoise        // Add noise perturbation
//#define weightSyndromes // NOT IMPLEMENTED Apply scale factor to weight syndrome sums
//#define outputSmoothing // Apply smoothing to the output
//#define thresholdAdaptation
//#define saturateSamples
//#define anneal // Dynamically reduce noise variance during decoding
//#define noiseShaping
*/

//--- Standard C++ headers ---//
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <sstream>
#include <time.h>
#include <stdio.h>

using namespace std;

//--- Borrowed from Radford Neal's source code ---//
#include "alist.h"
#include "rand_gsl.h"


//============ GLOBAL PARAMETERS ============//

int    num_iterations; // Maximum number of iterations for MLSBM (an additional phase of Gallager-A follows after this)
double theta;          // Threshold
double lambda = 0.991;  // Adaptation parameter
int    Tswitch = 0;
double alpha = 2.25;
double Ymax = 2.25;
int    windowsize = 64;
double noiseScale = 1.0;
int maxphase=7;
int NF=10;
int NR= 10;

char timeString[80];  


//============ DECODING ALGORITHM PREDEFINES ===============//
void checkNodeUpdates(alist_struct &H, vector<int> & sym_to_check, vector<int> & check_to_sym, bool & satisfied);
void symNodeUpdates(alist_struct &H, vector<double> &  thetas, double & lambda, int & mu,  vector<double> & y, vector<int> & d, vector<int> & check_to_sym, double & sigma, vector<double> & perturbation);
double evaluateObjectiveFunction(alist_struct &H, vector<int> & d, vector<double> & y, vector<int> & check_to_sym); 


//============= SUPPORTING FUNCTION PREDEFINES =================//
int find(int symNodes[], int len, int snode);
int countDecisionErrors(vector<int> d, vector<int> c);


//============= I/O PREDEFINES ============================//
void printHistogram(vector<int> & h);
void printVector(vector<int> v);
void fprintVector(ofstream & of, vector<int> & v);
void printVector(vector<double> v);
void printErroneousMessages(vector<vector<int> > v);
void writeErroneousMessagesToFile(alist_struct & H, vector<vector<int> > & check_to_sym, vector<vector<int> > & sym_to_check, vector<int> & c, vector<int> & d, int fid);
void writeErroneousMessagesToFile(alist_struct & H, vector<vector<int> > & check_to_sym, vector<vector<int> > & sym_to_check, vector<int> & c, vector<int> & d, vector<double> & y, vector<int> & yq, vector<int> & r, int fid, int it);
void recordRanState(int framenum, const gsl_rng * r);



///////////////////////////////////////////////////////////////////////////////
// -----===== MAIN BODY ======------
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char * argv[])
{
  vector<string> command_arguments(0);
  command_arguments.push_back("alist");
  command_arguments.push_back("R");
  command_arguments.push_back("SNR");
  command_arguments.push_back("T");
  command_arguments.push_back("NR");
  command_arguments.push_back("NF");
  command_arguments.push_back("theta");
  command_arguments.push_back("logfilename");
#ifdef addNoise
  command_arguments.push_back("noiseScale");
#endif
#ifdef thresholdAdaptation
  command_arguments.push_back("lambda");
#endif
#ifdef weightSyndromes
  command_arguments.push_back("alpha");
#endif
#ifdef outputSmoothing
  command_arguments.push_back("windowsize");
#endif
#ifdef saturateSamples
  command_arguments.push_back("Ymax");
#endif
//#ifdef redecode
  //command_arguments.push_back("maxphase");
//#endif
  command_arguments.push_back("[codeword filename]");

  // Check arguments and print usage statements:
  if ((argc != command_arguments.size()) && (argc != command_arguments.size()+1))
    {
      cout << "Usage: " << argv[0];
      for (int i=0; i<command_arguments.size(); i++)
	cout << " " << command_arguments[i];
      cout << "\n";
      return 0;
    }

  // Parse command arguments:
  int idx=1;
  alist_struct H = loadFile(argv[idx++]);
  cout << "PARAMETERS: \n alist = \t" << argv[1] << endl;
  double R = atof(argv[idx++]);
  cout << " R = \t" << R << endl;
  double SNR = atof(argv[idx++]);
  cout << " SNR = \t" << SNR << endl;
  num_iterations = atoi(argv[idx++]);
  cout << " T = \t" << num_iterations << endl;
  NR = atoi(argv[idx++]);
  cout << " NR = \t" << NR << endl;
  NF = atoi(argv[idx++]);
  cout << " NF = \t" << NF << endl;
  theta = atof(argv[idx++]);
  cout << " theta = \t" << theta << endl;
  string logfilename(argv[idx++]);
  cout << " log = \t" << logfilename << endl;

#ifdef addNoise
  noiseScale = atof(argv[idx++]);
  cout << " noiseScale = \t" << noiseScale << endl;
#endif
#ifdef thresholdAdaptation
  lambda = atof(argv[idx++]);
  cout << " lambda = \t" << lambda << endl;
#endif
#ifdef weightSyndromes
  alpha = atof(argv[idx++]);
  cout << " alpha = \t" << alpha << endl;
#endif
#ifdef outputSmoothing
  windowsize = atoi(argv[idx++]);
  cout << "windowsize = \t" << windowsize << endl;
#endif
#ifdef saturateSamples
  Ymax = atof(argv[idx++]);
  cout << " Ymax = \t" << Ymax << endl;
#endif
//#ifdef redecode   
  //maxphase = atoi(argv[idx++]);
  //cout << " maxphase = \t" << maxphase << endl;
//#endif
  ifstream codewordFile;
  if (argc == command_arguments.size()+1)
    {
      cout << "\nUsing codewords from " << argv[idx] << endl;
      codewordFile.open(argv[idx],ios::in);
    }
  else
    cout << "\nUsing all-zero sequence.\n";

  // Get Date/Time String:
  time_t rawtime;
  struct tm * timeinfo;
  time (&rawtime);
  timeinfo = localtime(&rawtime);

  strftime(timeString,80,"%d-%m-%Y",timeinfo);

  // Compute channel parameters:
  double N0 = pow(10.0,-SNR/10.0)/R;
  double sigma = sqrt(N0/2.0);

  // Get code parameters:
  int dv = H.biggest_num_n;
  int dc = H.biggest_num_m;

  // Report initial status messages:
  cout << "Simulating GDBF decoding on code with N=" << H.N << ", M=" << H.M << ", R=" << R << ", dv=" << dv << ", dc=" << dc << endl;
  cout << "\nParameters are:\n\tSNR\t" << SNR << "\n\tN0\t" << N0 << "\n\tsigma\t" << sigma << endl;


  // Declare top-level variables:
  vector<int>    c(H.N,1);     // Bipolar codeword (all +1 in this simulation)
  vector<double> x(H.N,1);     // Modulated codeword (all +1 in this simulation)
  vector<double> y(x);          // Channel samples
  vector<double> yq(H.N);       // Quantized channel samples
  vector<int>    r(H.N);        // Received bipolar decisions (+1 or -1)
  vector<int>    d(H.N,0);      // Decoder outputs (+1 or -1 after decoding)

#ifdef outputSmoothing
  vector<int> dsum(H.N,0);
  int smoothingUsed = 0;
#endif
//#ifdef redecode	
  int a,a1,a2,phase;
  vector<int> phase_hist(maxphase,0);  // Histogram for redecode phases
//#endif
  vector<double> perturbation(H.N,0.0);
  vector<double> noiseSamples(H.N,0.0);

  // Declare and initialize statistics variables:
  long errors = 0;            // Total bit errors
  long uncodedErrors = 0;     // Bit errors in r before decoding
  long totalBits = 0;         // Total number of bits observed
  long totalWords = 0;        // Total number of frames observed
  long wordErrors = 0;        // Number of word errors observed
  long totalIterations = 0;   // Total number of iterations accumulated over all frames.
  vector<int> error_weight_hist(H.N,0);  // Vector to serve as histogram of error-pattern weights (1 up to H.N)
  vector<double> thetas(H.N,theta);

  // NOTE: Could also do a histogram of the iteration count. It might be interesting.

  // Declare and initialize message memories:
  vector<int> check_to_sym(H.M,0);

  /////////////////////////////////////////////////////////////////
  // ------===== MAIN TEST LOOP =====-------
  /////////////////////////////////////////////////////////////////
  int minWordErrors = 20;
  if (H.N > 10000) minWordErrors = 10;
  if (H.N > 50000) minWordErrors = 5;
  ran_seed(time(0)); //(134159);
  int i,j;
  int framenum=0;
  //while ((errors < 200) || (wordErrors < minWordErrors))
    while (totalWords < NF)
    {
      recordRanState(framenum, rng_engine);
      string s;
      // If a codeword file is specified, load codewords from the file:
      if (argc == command_arguments.size()+1)
	{
	  getline(codewordFile, s);
	  if (codewordFile.eof())
	    {
	      codewordFile.clear();
	      codewordFile.seekg(0);
	      getline(codewordFile, s);
	    }
	  for (i=0; i<H.N; i++)
	    {	    
	      if (s[i] == '1')	      
		c[i] = -1;
	      else if (s[i] == '0')
		c[i] = +1;
	      else
		cout << "Got an invalid symbol at index " << i << endl;
	      x[i] = c[i];
	    }
	}
	
      bool satisfied;
      int it;
      int newErrors;

      // Emulate AWGN transmission      
      for (i=0; i<H.N; i++)
	{
	  y[i] = x[i]*(1.0+sigma*rann());


	#ifdef saturateSamples
	        if (abs(y[i])>Ymax)
		y[i] *= Ymax/abs(y[i]);
	    
	#endif

	  yq[i] = y[i];
	  if (yq[i] > 0)
	    r[i] = 1;
	  else
	    r[i] = -1;
	  d[i] = r[i];
	  if (r[i]*c[i] < 0)
	    uncodedErrors++;

#ifdef outputSmoothing
	      dsum[i] = 0;
#endif
	}

//#ifdef redecode
      phase=0;
      vector<int> outcomes(NR,0);
      int phase_iterations = num_iterations;
      while(phase < NR)
	{
	  for (i=0; i<H.N; i++)
	    {
	      d[i]=r[i];
#ifdef outputSmoothing
	      dsum[i] = 0;
#endif	    	
	    }
//#endif
	
  
#ifdef modeswitching
	  double f1, f2;
#endif

	  int mu;
#ifdef sequentialmode
	  mu = 0;
#else
	  mu=1;
#endif

#ifdef thresholdAdaptation
	  for (int i=0; i<H.N; i++)
	    thetas[i] = theta;
#endif

	  double noiseSigma = sigma*noiseScale;
#ifdef redecode
	  for (it=0; it<phase_iterations; it++)
#else
	    for (it=0; it<num_iterations; it++)
#endif
	      {      
		satisfied = true;
	  
	  
		// First update the check nodes:
		checkNodeUpdates(H,d,check_to_sym,satisfied);
		if (satisfied)
		  break;

	  
#ifdef modeswitching
		if (it > Tswitch)
		  f1 = evaluateObjectiveFunction(H,d,yq,check_to_sym);	    
#endif 
	  

	  
		// Then perform Symbol node updates:
	  
#ifdef addNoise
		for (int i=0; i<H.N; i++)
		  {
#ifdef uniformNoise
		    double newSample = sqrt(3)*noiseSigma*2.0*(ranu()-0.5);
#else
		    double newSample = noiseSigma*rann();
#endif
#ifdef noiseShaping
		    perturbation[i] = newSample - noiseSamples[i];
		    noiseSamples[i] = newSample;
#else	      
		    perturbation[i] = newSample;
#endif
		  }
#endif
	  

		symNodeUpdates(H,thetas,lambda, mu, yq, d,check_to_sym, noiseSigma, perturbation); 
	  
#ifdef modeswitching
		if (it > Tswitch)
		  {
		    f2 = evaluateObjectiveFunction(H,d,yq,check_to_sym);
		    if (f1 >= f2)
		      mu = 0;
		    //cout << "\tf2=" << f2 << "\t mu=" << mu << endl;
		  }
#endif

#ifdef outputSmoothing
		if (it > num_iterations-windowsize)
		  {
		    for (int i=0; i<H.N; i++)
		      dsum[i] += d[i];
		  }
#endif
	  
	      } // End of iteration loop (for one phase)
      
#ifdef outputSmoothing
	  if (!satisfied)
	    for (int i=0; i<H.N; i++)
	      {
		if (dsum[i] > 0)
		  d[i] = 1;
		else
		  d[i] = -1;
	      }
#endif
	  // --- End of iteration --------------------------------------
	  // -------------------------------------------------------------

	  // Count number of times smoothing is used:
#ifdef outputSmoothing
	  if (it > num_iterations-windowsize)
	    smoothingUsed++;
#endif

	  // Count remaining errors after decoding:
	  newErrors = countDecisionErrors(d,c);
	  totalIterations += it;
	  outcomes[phase] = newErrors;
	  
//#ifdef redecode
	  phase++;
	  //if(satisfied)
	    //break;
	}    // end of phase loop

      
      ofstream of(logfilename.c_str(),ios::app);
      of << framenum << "\t"; 
      fprintVector(of,outcomes);
      framenum++;
      of << std::endl;
      of.close();

      // Add code to record histogram of number of phases
      phase_hist[phase-1]++;
//#endif

      if (newErrors > 0)
	{
	  // Report the frame error to the console:
	  cout << "Ferr with " << newErrors << " errors.";
	  if (satisfied)
	    cout << " All checks satisfied.\n";
	  else
	    cout << endl;

	  // Update statistical information
	  errors += newErrors;
	  error_weight_hist[newErrors-1]++;
	  wordErrors++;
	
	  
	}



      // Increment frame and bit counters:
      totalBits += H.N; 
      //      totalIterations += it;
      totalWords++;
      // ------------------------------------------------
      // Give a status message every 100 frames
      int reportInterval = round(100e3/H.N);
      if ((totalWords % reportInterval) == 0)
	{
	  cout << "\nIncremental result: " << errors << " bit errs in " << totalWords << " words, BER=" << (double)errors/totalBits 
	       << ". Average iterations = " << (double) totalIterations/totalWords << ". Word error=" << wordErrors << ". Uncoded errors = " << uncodedErrors << ", uncBER=" << (double)uncodedErrors/totalBits
	       << "\nError weights:\n";
	  printHistogram(error_weight_hist);
//#ifdef redecode
	  cout<<"Phase histogram:\n";
	  printHistogram(phase_hist);
//#endif
	}
      // ------------------------------------------------

    }
  /////////////////////////////////////////////////////////////////
  // ------===== END OF MAIN TEST LOOP =====-------
  /////////////////////////////////////////////////////////////////
  
  // ------------------------------------------------
  // REPORT FINAL RESULTS:
  cout << "\nFinal result: " << errors << " bit errs in " 
       << totalWords << " words, BER=" << (double)errors/(totalBits)<< ". Average iterations = " << (double) totalIterations/totalWords 
       << ". Uncoded errors = " << uncodedErrors << ", uncBER=" 
       << (double)uncodedErrors/totalBits << endl;      
//#ifdef redecode
  cout<<"Phase histogram:\n"<<endl;
  printHistogram(phase_hist);      
//#endif

  /*ofstream of(logfilename.c_str(),ios::app);
  char tab = '\t';
  of << SNR << tab << (double)errors/totalBits << tab << (double) totalIterations/totalWords << tab
     << (double) wordErrors/totalWords << tab
     << totalBits << tab << totalWords << tab
     << num_iterations << tab << theta << tab;
#ifdef addNoise
  of << noiseScale << tab; 
#endif
#ifdef thresholdAdaptation
  of << lambda << tab; 
#endif
#ifdef weightSyndromes
  of << alpha << tab;
#endif
#ifdef outputSmoothing
  of << smoothingUsed << tab << (double) smoothingUsed/totalWords << tab;
  of << windowsize << tab; 
#endif
#ifdef saturateSamples
  of << Ymax << tab;
#endif
#ifdef redecode
  of << maxphase << tab;
#endif
  of << argv[1]
     << endl;
*/
  return 0;
}
/////////////////////////////////////////////////////////////////
// ------===== END OF MAIN BODY =====-------
/////////////////////////////////////////////////////////////////


//============================================================//
// Functions follow in no particular order, and without
// adequate comments...
//============================================================//

void printHistogram(vector<int> & h)
{
  for (int i=0; i<h.size(); i++)
    {
      if (h[i] > 0)
	cout << i+1 << ":\t" << h[i] << endl;
    }
}

int countDecisionErrors(vector<int> d, vector<int> c)
{
  int errs =0 ;
  for (int i=0; i<d.size(); i++)
    {
      if ((isnan(d[i])) || (d[i] == 0))
	cout << "Problem decision at index " << i << " = " << d[i] << endl;
      if (d[i] != c[i])	
	errs++;
    }
  return errs;
}




void printErroneousMessages(vector<vector<int> > v)
{
  for (int i=0; i<v.size(); i++)
    {
      for (int j=0; j<v[i].size(); j++)
	{
	  if (v[i][j] < 0)
	    cout << " " << i << "." << j;
	}
    }
}

void checkNodeUpdates(alist_struct &H, vector<int> & sym_to_check, vector<int> & check_to_sym, bool & satisfied)
{
  int msg;
  satisfied = true;
  for (int i=0; i<H.M; i++)
    {
      int prod = 1;
      for (int j=0; j<H.num_mlist[i]; j++)
	{
	  int snode = H.mlist[i][j]-1;
	  msg = sym_to_check[snode];
	  prod *= msg;
	}
      if (prod < 0)
	satisfied = false;
      check_to_sym[i] = prod;	
    }
}

void symNodeUpdates(alist_struct &H, vector<double> & thetas, double & lambda, int & mu, vector<double> & y, vector<int> & d, vector<int> & check_to_sym, double & sigma, vector<double> & perturbation)
{
  vector<double> E(H.N,0.0);
  double Emin = INFINITY;
  int mindx = -1;
  double w = 1;
  
  for (int i=0; i<H.N; i++)
    {
      bool flip = false;
      E[i] = d[i]*y[i];

#ifdef weightSyndromes
      int dv = H.num_nlist[i];
      w = alpha*Ymax/dv;
#endif

      for (int j=0; j<H.num_nlist[i]; j++)
	{
	  int cnode = H.nlist[i][j]-1;
	  int msg = check_to_sym[cnode];
	  E[i] += w*msg;	  
	}      
#ifdef addNoise
      E[i] += perturbation[i]; //sigma*rann();
#endif
      if ((mu == 1) && (E[i] < thetas[i]))
	{
	  flip = true;
	  d[i] = -d[i];      	    
	}
      if (mu == 0)	
	if (E[i] < Emin)
	  {
	    flip = true;
	    Emin = E[i];
	    mindx = i;
	  }	
#ifdef thresholdAdaptation
      if (flip)
	thetas[i] = thetas[i]; // /= lambda;
      else
	thetas[i] *= lambda;
#endif
    }
  if ((mu == 0)&&(mindx>=0))
    d[mindx] = -d[mindx];
}


double evaluateObjectiveFunction(alist_struct &H, vector<int> & d, vector<double> & y, vector<int> & check_to_sym)
{
  double f = 0;
  for (int i=0; i<H.N; i++)
    f += d[i]*y[i];
  for (int j=0; j<H.M; j++)
    f += check_to_sym[j];

  return f;
}


int find(int symNodes[], int len, int snode)
{
  int result = -1;
  for (int i=0; i<len; i++)
    {
      if (snode == (symNodes[i]-1))
	result = i;      
    }
  return result;
}

void printVector(vector<int> v)
{
  for (int i=0; i<v.size(); i++)
    {
      cout << v[i] << "\t";
    }
}      

void fprintVector(ofstream & of, vector<int> & v)
{
  for (int i=0; i<v.size(); i++)
    {
      of << v[i] << "\t";
    }
}                                                                                                                                   
 
                                                                                                                                                                                             
void printVector(vector<double> v)
{
  for (int i=0; i<v.size(); i++)
    {
      cout << v[i] << "\t";
    }
}                                           



void writeErroneousMessagesToFile(alist_struct & H, vector<vector<int> > & check_to_sym, vector<vector<int> > & sym_to_check, vector<int> & c, vector<int> & d, int fid)
{
#ifdef erroneousMessageFile
  stringstream ss;
  ss << erroneousMessageFile << "_" << fid << ".dat";
  ofstream f(ss.str().c_str(),ios::out);

  int i,j,k;
  for (i=0; i<H.N; i++)
    {
      int correct_msg = c[i];
      if (d[i] != correct_msg)
	f << "Dec err at node " << i << endl;
      bool anyErr = false;
      for (j=0; j<H.num_nlist[i]; j++)
	{
	  int msg = sym_to_check[i][j];
	  if (msg != correct_msg)
	    {	      
	      int cnode = H.nlist[i][j]-1;
	      int midx = find(H.mlist[cnode],H.num_mlist[cnode],i);
	      f << "\t StoC err, S " << i << " -> C " << cnode << endl;
	      for (k=0; k<H.num_mlist[cnode]; k++)
		{
		  if (k != midx)
		    {
		      int cmsg = check_to_sym[cnode][k];
		      int snode = H.mlist[cnode][k]-1;
		      int midx2 = find(H.nlist[snode], H.num_nlist[snode], cnode); 
		      int msg2 = sym_to_check[snode][midx2];
		      if (msg2 != c[snode])
			f << "\t\t Ck node " << cnode << " got err from Sym node " << snode << endl;
		    }
		}
	    }	  
	}      
    }
  f.close();
#endif
}


void writeErroneousMessagesToFile(alist_struct & H, vector<vector<int> > & check_to_sym, vector<vector<int> > & sym_to_check, vector<int> & c, vector<int> & d, vector<double> & y, vector<int> & yq, vector<int> & r, int fid, int it)
{
#ifdef erroneousMessageFile
  stringstream ss;
  ss << "detail_" << erroneousMessageFile << "_" << fid << ".dat";
  ofstream f;
  if (it == 0)
    f.open(ss.str().c_str(),ios::out);
  else
    f.open(ss.str().c_str(),ios::app);
  int i,j,k;

  f << "\n\nITERATION " << it << endl;
  for (i=0; i<H.N; i++)
    {
      int correct_msg = c[i];
      if (d[i] != correct_msg)
	f << "Dec err at node " << i << " y=" << y[i] << ", yq=" << yq[i] << ", r=" << r[i] << ", d=" << d[i] << ", c=" << c[i] << endl;
      bool anyErr = false;
      for (j=0; j<H.num_nlist[i]; j++)
	{
	  int msg = sym_to_check[i][j];
	  if (msg != correct_msg)
	    {	      
	      int cnode = H.nlist[i][j]-1;
	      int midx = find(H.mlist[cnode],H.num_mlist[cnode],i);
	      f << "\t StoC err, S " << i << " -> C " << cnode << endl;
	      for (k=0; k<H.num_mlist[cnode]; k++)
		{
		  if (k != midx)
		    {
		      int cmsg = check_to_sym[cnode][k];
		      int snode = H.mlist[cnode][k]-1;
		      int midx2 = find(H.nlist[snode], H.num_nlist[snode], cnode); 
		      int msg2 = sym_to_check[snode][midx2];
		      if (msg2 != c[snode])
			f << "\t\t Ck node " << cnode << " got err from Sym node " << snode << endl;
		    }
		}
	    }	  
	}      
    }
  f.close();
#endif
}


void recordRanState(int framenum, const gsl_rng * r)
{
  stringstream ss("");
  ss << "tmp/" << timeString  << "_RanState_" << framenum << ".state";
  FILE * state = fopen(ss.str().c_str(),"w+");
  
  gsl_rng_fwrite (state, r);
  fclose(state);
}
