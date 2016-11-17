/*
  Copyright (c) 2014 - 2016
  CLST  - Radboud University
  ILK   - Tilburg University

  This file is part of foliautils

  foliautils is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  foliautils is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      https://github.com/LanguageMachines/foliautils/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl
*/

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>

#include "ticcutils/CommandLine.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "libfolia/folia.h"

#include "config.h"
#ifdef HAVE_OPENMP
#include "omp.h"
#endif

using namespace	std;
using namespace	folia;
using namespace	TiCC;

bool verbose = false;
string classname = "current";

enum Mode { UNKNOWN,
	    T_IN_D, T_IN_P, T_IN_S,
	    S_IN_D, S_IN_P,
	    W_IN_D, W_IN_P, W_IN_S  };

Mode stringToMode( const string& ms ){
  if ( ms == "text_in_doc" ){
    return T_IN_D;
  }
  else if ( ms == "text_in_par" ){
    return T_IN_P;
  }
  else if ( ms == "text_in_sent" ){
    return T_IN_S;
  }
  else if ( ms == "string_in_doc" ){
    return S_IN_D;
  }
  else if ( ms == "string_in_par" ){
    return S_IN_P;
  }
  else if ( ms == "word_in_doc" ){
    return W_IN_D;
  }
  else if ( ms == "word_in_par" ){
    return W_IN_P;
  }
  else if ( ms == "word_in_sent" ){
    return W_IN_S;
  }
  return UNKNOWN;
}

void create_wf_list( const map<string, unsigned int>& wc,
		     const string& filename, unsigned int totalIn,
		     unsigned int clip,
		     bool doperc ){
  unsigned int total = totalIn;
  ofstream os( filename );
  if ( !os ){
    cerr << "failed to create outputfile '" << filename << "'" << endl;
    exit(EXIT_FAILURE);
  }
  map<unsigned int, set<string> > wf;
  map<string,unsigned int >::const_iterator cit = wc.begin();
  while( cit != wc.end()  ){
    if ( cit->second <= clip ){
      total -= cit->second;
    }
    else {
      wf[cit->second].insert( cit->first );
    }
    ++cit;
  }
  unsigned int sum=0;
  unsigned int types=0;
  map<unsigned int, set<string> >::const_reverse_iterator wit = wf.rbegin();
  while ( wit != wf.rend() ){
    set<string>::const_iterator sit = wit->second.begin();
    while ( sit != wit->second.end() ){
      sum += wit->first;
      os << *sit << "\t" << wit->first;
      if ( doperc ){
	os << "\t" << sum << "\t" << 100 * double(sum)/total;
      }
      os << endl;
      ++types;
      ++sit;
    }
    ++wit;
  }
#pragma omp critical
  {
    cout << "created WordFreq list '" << filename << "'";
    if ( clip > 0 ){
      cout << endl << "with " << total << " words and " << types << " types. (" << totalIn - total
	   << " of the original " << totalIn << " words were clipped.)" << endl;
    }
    else {
      cout << " for " << total << " word tokens." << endl;
    }
  }
}

struct rec {
  unsigned int count;
  map<string,unsigned int> pc;
};

void create_lf_list( const map<string, unsigned int>& lc,
		     const string& filename, unsigned int totalIn,
		     unsigned int clip,
		     bool doperc ){
  unsigned int total = totalIn;
  ofstream os( filename );
  if ( !os ){
    cerr << "failed to create outputfile '" << filename << "'" << endl;
    exit(EXIT_FAILURE);
  }
  map<unsigned int, set<string> > lf;
  map<string,unsigned int >::const_iterator cit = lc.begin();
  while( cit != lc.end()  ){
    if ( cit->second <= clip ){
      total -= cit->second;
    }
    else {
      lf[cit->second].insert( cit->first );
    }
    ++cit;
  }

  unsigned int sum=0;
  unsigned int types=0;
  map<unsigned int, set<string> >::const_reverse_iterator wit = lf.rbegin();
  while ( wit != lf.rend() ){
    set<string>::const_iterator sit = wit->second.begin();
    while ( sit != wit->second.end() ){
      sum += wit->first;
      os << *sit << "\t" << wit->first;
      if ( doperc ){
	os << "\t" << sum << "\t" << 100* double(sum)/total;
      }
      os << endl;
      ++types;
      ++sit;
    }
    ++wit;
  }
#pragma omp critical
  {
    cout << "created LemmaFreq list '" << filename << "'";
    if ( clip > 0 ){
      cout << endl << "with " << total << " lemmas and " << types << " types. (" << totalIn - total
	   << " of the original " << totalIn << " lemmas were clipped.)" << endl;
    }
    else {
      cout << " for " << total << " lemmas. " << endl;
    }
  }
}

void create_lpf_list( const multimap<string, rec>& lpc,
		      const string& filename, unsigned int totalIn,
		      unsigned int clip,
		      bool doperc ){
  unsigned int total = totalIn;
  ofstream os( filename );
  if ( !os ){
    cerr << "failed to create outputfile '" << filename << "'" << endl;
    exit(EXIT_FAILURE);
  }
  multimap<unsigned int, pair<string,string> > lpf;
  multimap<string,rec>::const_iterator cit = lpc.begin();
  while( cit != lpc.end()  ){
    map<string,unsigned int>::const_iterator pit = cit->second.pc.begin();
    while ( pit != cit->second.pc.end() ){
      if ( pit->second <= clip ){
	total -= pit->second;
      }
      else {
	lpf.insert( make_pair( pit->second,
			       make_pair( cit->first, pit->first ) ) );
      }
      ++pit;
    }
    ++cit;
  }
  unsigned int sum =0;
  unsigned int types =0;
  multimap<unsigned int, pair<string,string> >::const_reverse_iterator wit = lpf.rbegin();
  while ( wit != lpf.rend() ){
    sum += wit->first;
    os << wit->second.first << " " << wit->second.second << "\t" << wit->first;
    if ( doperc ){
      os << "\t" << sum << "\t" << 100 * double(sum)/total;
    }
    os << endl;
    ++types;
    ++wit;
  }
#pragma omp critical
  {
    cout << "created LemmaPosFreq list '" << filename << "'";
    if ( clip > 0 ){
      cout << endl << "with " << total << " lemmas and " << types << " types. (" << totalIn - total
	   << " of the original " << totalIn << " lemmas were clipped.)" << endl;
    }
    else {
      cout << " for " << totalIn << " lemmas. " << endl;
    }
  }
}

struct wlp_rec {
  string word;
  string lemma;
  string pos;
};

const string frog_cgntagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
const string frog_mblemtagset = "http://ilk.uvt.nl/folia/sets/frog-mblem-nl";

template<class F>
void taal_filter( vector<F*>& words,
		  const string& taal ){
  typename vector<F*>::iterator it = words.begin();
  while ( it != words.end() ){
    string lang = (*it)->language(classname);
    if ( !lang.empty()
	 && lang.find( taal ) == string::npos ){
      // there is language info, and ist the wrong language
      it = words.erase( it );
    }
    else {
      ++it;
    }
  }
}

bool is_emph( const string& data ){
  return (data.size() < 2) && isalnum(data[0]);
}

void add_emph_inventory( const vector<string>& data,
			 set<string>& emph ){
  for ( unsigned int i=0; i < data.size(); ++i ){
    bool done = false;
    for ( unsigned int j=i; j < data.size() && !done; ++j ){
      if ( is_emph( data[j] ) ){
	// a candidate?
	if ( j + 1 < data.size()
	     && is_emph( data[j+1] ) ){
	  // yes a second short word
	  string mw = data[j] + "_" + data[j+1];
	  for ( unsigned int k=j+2; k < data.size(); ++k ){
	    if ( is_emph(data[k]) ){
	      mw += "_" + data[k];
	    }
	    else {
	      emph.insert(mw);
	      mw.clear();
	      i = k; // restart i loop there
	      done = true; // get out of j loop
	      break; // k loop
	    }
	  }
	}
      }
    }
  }
}

void add_emph_inventory( vector<wlp_rec>& data, set<string>& emph ){
  for ( unsigned int i=0; i < data.size(); ++i ){
    string mw;
    bool first = true;
    for ( unsigned int j=i; j < data.size(); ++j ){
      if ( data[j].word.size() < 2 ){
	if ( !first ){
	  mw += "_";
	}
	first = false;
	mw += data[j].word;
      }
      else {
	emph.insert(mw);
	mw.clear();
	first = true;
      }
    }
  }
}

size_t add_word_inventory( const vector<string>& data,
			   map<string,unsigned int>& wc,
			   size_t nG,
			   const string& sep ){
  size_t count = 0;
  if ( data.size() < nG ){
    return 0;
  }
  for ( unsigned int i=0; i <= data.size() - nG ; ++i ){
    string multiw;
    for ( size_t j=0; j < nG; ++j ){
      multiw += data[i+j];
      if ( j < nG-1 ){
	multiw += sep;
      }
    }
    ++count;
#pragma omp critical
    {
      ++wc[multiw];
    }
  }
  return count;
}

size_t doc_word_inventory( const Document *d, const string& docName,
			   size_t nG,
			   bool lowercase,
			   const string& lang,
			   map<string,unsigned int>& wc,
			   map<string,unsigned int>& lc,
			   multimap<string, rec>& lpc,
			   unsigned int& lemTotal,
			   unsigned int& posTotal,
			   set<string>& emph,
			   const string& sep ){
  if ( verbose ){
#pragma omp critical
    {
      cerr << "make a word_str inventory on:" << docName << endl;
    }
  }
  size_t wordTotal = 0;
  lemTotal = 0;
  posTotal = 0;
  vector<Sentence *> sents = d->sentences();
  if ( verbose ){
#pragma omp critical
    {
      cerr << docName <<  ": " << sents.size() << " sentences" << endl;
    }
  }
  for ( unsigned int s=0; s < sents.size(); ++s ){
    vector<Word*> words = sents[s]->words();
    if ( verbose ){
#pragma omp critical
      {
	cerr << docName <<  " sentence-" << s << " :" << words.size() << "words" << endl;
      }
    }
    taal_filter( words, lang );
    if ( verbose ){
#pragma omp critical
      {
	cerr << docName <<  " sentence-" << s << " after language filter :" << words.size() << "words" << endl;
      }
    }
    if ( words.size() < nG )
      continue;
    vector<wlp_rec> data;
    for ( unsigned int i=0; i < words.size(); ++i ){
      wlp_rec rec;
      try {
	UnicodeString uword = words[i]->text(classname);
	if ( lowercase ){
	  uword.toLower();
	}
	rec.word = UnicodeToUTF8( uword );
      }
      catch(...){
#pragma omp critical
	{
	  cerr << "missing text for word " << words[i]->id() << endl;
	}
	break;
      }
      try {
	rec.lemma = words[i]->lemma(frog_mblemtagset);
      }
      catch(...){
	try {
	  rec.lemma = words[i]->lemma();
	}
	catch(...){
	  rec.lemma = "";
	}
      }
      try {
	rec.pos = words[i]->pos(frog_cgntagset);
      }
      catch(...){
	try {
	  rec.pos = words[i]->pos();
	}
	catch (...){
	  rec.pos = "";
	}
      }
      data.push_back( rec );
    }
    if ( data.size() != words.size() ) {
#pragma omp critical
      {
	cerr << "Error: Missing words! skipped sentence " << sents[s]->id() << " in " << docName << endl;
      }
      continue;
    }

    add_emph_inventory( data, emph );
    for ( unsigned int i=0; i <= data.size() - nG ; ++i ){
      string multiw;
      string multil;
      string multip;
      bool lem_mis = false;
      bool pos_mis = false;
      for ( size_t j=0; j < nG; ++j ){
	multiw += data[i+j].word;
	if ( data[i+j].lemma.empty() ){
	  lem_mis = true;
	}
	else if ( !lem_mis ){
	  multil += data[i+j].lemma;
	}
	if ( data[i+j].pos.empty() ){
	  pos_mis = true;
	}
	else if ( !pos_mis ){
	  multip += data[i+j].pos;
	}
	if ( j < nG-1 ){
	  multiw += sep;
	  if ( !lem_mis )
	    multil += sep;
	  if ( !pos_mis )
	    multip += sep;
	}
      }
      ++wordTotal;
      if ( !lem_mis ){
	++lemTotal;
      }
      if ( !pos_mis ){
	++posTotal;
      }
#pragma omp critical
      {
	++wc[multiw];
      }

      if ( !multil.empty() ){
#pragma omp critical
	{
	  ++lc[multil];
	}
      }
      if ( !multip.empty() ){
#pragma omp critical
	{
	  multimap<string, rec >::iterator it = lpc.find(multil);
	  if ( it == lpc.end() ){
	    rec tmp;
	    tmp.count = 1;
	    tmp.pc[multip]=1;
	    lpc.insert( make_pair(multil,tmp) );
	  }
	  else {
	    ++it->second.count;
	    ++it->second.pc[multip];
	  }
	}
      }
    }
  }
  if ( verbose && (lemTotal < wordTotal) ){
#pragma omp critical
    {
      cerr << "info: " << wordTotal - lemTotal
	   << " lemma's are missing in "  << d->id() << endl;
    }
  }
  if ( verbose && (posTotal < wordTotal) ){
#pragma omp critical
    {
      cerr << "info: " << wordTotal - posTotal
	   << " POS tags are missing in "  << d->id() << endl;
    }
  }
  return wordTotal;
}

size_t doc_str_inventory( const Document *d, const string& docName,
			  size_t nG,
			  bool lowercase,
			  const string& lang,
			  map<string,unsigned int>& wc,
			  set<string>& emph,
			  const string& sep ){
  if ( verbose ){
#pragma omp critical
    {
      cerr << "make a str inventory on:" << docName << endl;
    }
  }
  size_t wordTotal = 0;
  vector<String*> strings = d->doc()->select<String>();
  if ( verbose ){
#pragma omp critical
    {
      cerr << "found " << strings.size() << " strings" << endl;
    }
  }

  taal_filter( strings, lang );
  if ( verbose ){
#pragma omp critical
    {
      cerr << "after filter on " << lang << " " << strings.size() << " strings" << endl;
    }
  }
  if ( strings.size() < nG )
    return 0;
  vector<string> data;
  for ( unsigned int i=0; i < strings.size(); ++i ){
    UnicodeString us;
    try {
      us = strings[i]->text(classname);
      if ( lowercase ){
	us.toLower();
      }
    }
    catch(...){
#pragma omp critical
      {
	cerr << "missing text for word " << strings[i]->id() << endl;
      }
      break;
    }
    string w = UnicodeToUTF8( us );
    data.push_back( w );
  }
  if ( data.size() != strings.size() ) {
#pragma omp critical
    {
      cerr << "Error: Missing words! skipped document " << docName << endl;
    }
    return 0;
  }

  add_emph_inventory( data, emph );
  wordTotal += add_word_inventory( data, wc, nG, sep );
  return wordTotal;
}

size_t par_str_inventory( const Document *d, const string& docName,
			  size_t nG,
			  bool lowercase,
			  const string& lang,
			  map<string,unsigned int>& wc,
			  set<string>& emph,
			  const string& sep ){
  if ( verbose ){
#pragma omp critical
    {
      cerr << "make a par_str inventory on:" << docName << endl;
    }
  }
  size_t wordTotal = 0;
  vector<Paragraph*> pars = d->paragraphs();
  for ( unsigned int p=0; p < pars.size(); ++p ){
    vector<String*> strings = pars[p]->select<String>();
    if ( verbose ){
#pragma omp critical
      {
	cerr << "found " << strings.size() << " strings" << endl;
      }
    }
    taal_filter( strings, lang );
    if ( verbose ){
#pragma omp critical
      {
	cerr << "after filter on " << lang << " " << strings.size() << " strings" << endl;
      }
    }
    if ( strings.size() < nG )
      continue;
    vector<string> data;
    for ( unsigned int i=0; i < strings.size(); ++i ){
      UnicodeString us;
      try {
	us = strings[i]->text(classname);
	if ( lowercase ){
	  us.toLower();
	}
      }
      catch(...){
#pragma omp critical
	{
	  cerr << "missing text for word " << strings[i]->id() << endl;
	}
      break;
      }
      string w = UnicodeToUTF8( us );
      data.push_back( w );
    }
    if ( data.size() != strings.size() ) {
#pragma omp critical
      {
	cerr << "Error: Missing words! skipped paragraph " << pars[p]->id() << " in " << docName << endl;
      }
      continue;
    }

    add_emph_inventory( data, emph );
    wordTotal += add_word_inventory( data, wc, nG, sep );
  }
  return wordTotal;
}

size_t par_text_inventory( const Document *d, const string& docName,
			   size_t nG,
			   bool lowercase,
			   const string& lang,
			   map<string,unsigned int>& wc,
			   set<string>& emph,
			   const string& sep ){
  if ( verbose ){
#pragma omp critical
    {
      cerr << "make a text_in_par inventory on:" << docName << endl;
    }
  }
  size_t wordTotal = 0;
  vector<Paragraph*> pars = d->paragraphs();
  for ( unsigned int p=0; p < pars.size(); ++p ){
    string p_lang = pars[p]->language();
    if ( p_lang != lang && lang != "none" ){
      if ( verbose ){
#pragma omp critical
	{
	  cerr << "skip a paragraph in wrong language: " << p_lang << endl;
	}
      }
      continue;
    }
    string s;
    UnicodeString us;
    try {
      us = pars[p]->text(classname);
      if ( lowercase ){
	us.toLower();
      }
      s = UnicodeToUTF8( us );
    }
    catch(...){
    }
    if ( s.empty() ){
      if ( verbose ){
#pragma omp critical
	{
	  cerr << "found NO string in paragraph " << p << " (" << pars[p]->id() << ")" << endl;
	}
      }
      continue;
    }
    vector<string> data;
    size_t num = TiCC::split( s, data );
    if ( verbose ){
#pragma omp critical
      {
	cerr << "found string: '" << s << "'" << endl;
	if ( num <= 1 ){
	  cerr << "with no substrings" << endl;
	}
	else {
	  using TiCC::operator<<;
	  cerr << "with " << num << " substrings: " << data << endl;
	}
      }
    }
    add_emph_inventory( data, emph );
    wordTotal += add_word_inventory( data, wc, nG, sep );
  }
  return wordTotal;
}

void usage( const string& name ){
  cerr << "Usage: " << name << " [options] file/dir" << endl;
  cerr << "\t\t FoLiA-stats will produce ngram statistics for a FoLiA file, " << endl;
  cerr << "\t\t or a whole directory of FoLiA files " << endl;
  cerr << "\t\t The output will be a 2 or 4 columned tab separated file, extension: *tsv " << endl;
  cerr << "\t\t\t (4 columns when -p is specified)" << endl;
  cerr << "\t--clip\t\t clipping factor. " << endl;
  cerr << "\t\t\t\t(entries with frequency <= this factor will be ignored). " << endl;
  cerr << "\t-p\t\t output percentages too. " << endl;
  cerr << "\t--lower\t\t Lowercase all words" << endl;
  cerr << "\t--underscore\t connect all words with underscores" << endl;
  cerr << "\t--lang\t\t Language. (default='dut'). 'none' is also possible" << endl;
  cerr << "\t--ngram\t\t Ngram count " << endl;
  cerr << "\t-s\t\t Process <str> nodes not <w> per <p> node" << endl;
  cerr << "\t-S\t\t Process <str> nodes not <w> per document" << endl;
  cerr << "\t--class='name'\t When processing <str> nodes, use 'name' as the folia class for <t> nodes. (default is 'OCR')" << endl;
  cerr << "\t--hemp=<file>\t Create a historical emphasis file. " << endl;
  cerr << "\t\t\t (words consisting of single, space separated letters)" << endl;
  cerr << "\t-t\t\t number_of_threads" << endl;
  cerr << "\t-h or --help\t this message" << endl;
  cerr << "\t-v or --verbose\t very verbose output." << endl;
  cerr << "\t-V or --version\t show version " << endl;
  cerr << "\t-e\t\t expr: specify the expression all input files should match with." << endl;
  cerr << "\t-o\t\t name of the output file(s) prefix." << endl;
  cerr << "\t-R\t\t search the dirs recursively (when appropriate)." << endl;
}

int main( int argc, char *argv[] ){
  CL_Options opts( "hVvpe:t:o:RsS",
		   "class:,clip:,lang:,ngram:,lower,hemp:,underscore,help,version,mode:,verbose" );
  try {
    opts.init(argc,argv);
  }
  catch( OptionError& e ){
    cerr << e.what() << endl;
    usage(argv[0]);
    exit( EXIT_FAILURE );
  }
  string progname = opts.prog_name();
  if ( argc < 2 ){
    usage( progname );
    exit(EXIT_FAILURE);
  }
  int clip = 0;
  int nG = 1;
#ifdef HAVE_OPENMP
  int numThreads = 1;
#endif
  string expression;
  string outputPrefix;
  string lang = "dut";
  string value;
  if ( opts.extract('V') || opts.extract("version") ){
    cerr << PACKAGE_STRING << endl;
    exit(EXIT_SUCCESS);
  }
  if ( opts.extract('h') || opts.extract("help") ){
    usage(progname);
    exit(EXIT_SUCCESS);
  }
  verbose = opts.extract( 'v' ) || opts.extract( "verbose" );
  bool dopercentage = opts.extract('p');
  bool lowercase = opts.extract("lower");
  string modes;
  opts.extract("mode", modes );
  Mode mode = W_IN_S;
  if ( !modes.empty() ){
    mode = stringToMode( modes );
    if ( mode == UNKNOWN ){
      cerr << "unknown --mode " << modes << endl;
      return EXIT_FAILURE;
    }
  }
  else {
    mode = W_IN_D;
  }
  string hempName;
  opts.extract("hemp", hempName );
  bool recursiveDirs = opts.extract( 'R' );
  if ( opts.extract( 's' ) ) {
    if ( !modes.empty() ){
      cerr << "old style -s option cannot be combined with --mode option" << endl;
      return EXIT_FAILURE;
    }
    else {
      mode = S_IN_P;
    }
  }
  if ( opts.extract( 'S' ) ){
    if ( !modes.empty() ){
      cerr << "old style -S option cannot be combined with --mode option" << endl;
      return EXIT_FAILURE;
    }
    else {
      mode = S_IN_D;
    }
  }
  bool do_under = opts.extract( "underscore" );
  string sep = " ";
  if ( do_under ){
    sep = "_";
  }
  if ( !opts.extract( 'o', outputPrefix ) ){
    cerr << "an output filename prefix is required. (-o option) " << endl;
    exit(EXIT_FAILURE);
  }
  if ( opts.extract("clip", value ) ){
    if ( !stringTo(value, clip ) ){
      cerr << "illegal value for --clip (" << value << ")" << endl;
      exit(EXIT_FAILURE);
    }
  }
  if ( opts.extract("ngram", value ) ){
    if ( !stringTo(value, nG ) ){
      cerr << "illegal value for --ngram (" << value << ")" << endl;
      exit(EXIT_FAILURE);
    }
  }
  if ( opts.extract('t', value ) ){
#ifdef HAVE_OPENMP
    if ( !stringTo(value, numThreads ) ){
      cerr << "illegal value for -t (" << value << ")" << endl;
      exit(EXIT_FAILURE);
    }
#else
    cerr << "OpenMP support is missing. -t option is not supported" << endl;
    exit( EXIT_FAILURE );
#endif
  }
  if ( opts.extract("lang", value ) ){
    if ( value == "none" )
      lang.clear();
    else
      lang = value;
  }
  opts.extract('e', expression );
  opts.extract( "class", classname );
  if ( !opts.empty() ){
    cerr << "unsupported options : " << opts.toString() << endl;
    usage(progname);
    exit(EXIT_FAILURE);
  }

#ifdef HAVE_OPENMP
  omp_set_num_threads( numThreads );
#endif

  vector<string> massOpts = opts.getMassOpts();
  if ( massOpts.empty() ){
    cerr << "no file or dir specified!" << endl;
    exit(EXIT_FAILURE);
  }
  string name = massOpts[0];
  vector<string> fileNames = searchFilesMatch( name, expression, recursiveDirs );
  size_t toDo = fileNames.size();
  if ( toDo == 0 ){
    cerr << "no matching files found" << endl;
    exit(EXIT_SUCCESS);
  }

  string::size_type pos = outputPrefix.find( "." );
  if ( pos != string::npos && pos == outputPrefix.length()-1 ){
    // outputname ends with a .
    outputPrefix = outputPrefix.substr(0,pos);
  }
  pos = outputPrefix.find( "/" );
  if ( pos != string::npos && pos == outputPrefix.length()-1 ){
    // outputname ends with a /
    outputPrefix += "foliastats";
  }

  if ( toDo > 1 ){
    cout << "start processing of " << toDo << " files " << endl;
  }
  map<string,unsigned int> wc;
  map<string,unsigned int> lc;
  multimap<string, rec> lpc;
  unsigned int wordTotal =0;
  unsigned int posTotal =0;
  unsigned int lemTotal =0;
  set<string> emph;

#pragma omp parallel for shared(fileNames,wordTotal,posTotal,lemTotal,wc,lc,lpc,emph)
  for ( size_t fn=0; fn < fileNames.size(); ++fn ){
    string docName = fileNames[fn];
    Document *d = 0;
    try {
      d = new Document( "file='"+ docName + "'" );
    }
    catch ( exception& e ){
#pragma omp critical
      {
	cerr << "failed to load document '" << docName << "'" << endl;
	cerr << "reason: " << e.what() << endl;
      }
      continue;
    }
    unsigned int word_count = 0;
    unsigned int lem_count = 0;
    unsigned int pos_count = 0;
    switch ( mode ){
    case S_IN_P:
      word_count = par_str_inventory( d, docName, nG, lowercase, lang, wc, emph, sep );
      break;
    case T_IN_P:
      word_count = par_text_inventory( d, docName, nG, lowercase,
				       lang, wc, emph, sep );
      break;
    case S_IN_D:
      word_count = doc_str_inventory( d, docName, nG, lowercase,
				      lang, wc, emph, sep );
      break;
    case W_IN_D:
      word_count = doc_word_inventory( d, docName, nG, lowercase,
				       lang, wc, lc, lpc, lem_count,
				       pos_count, emph, sep );
      break;
    default:
      cerr << "not yet implemented mode: " << modes << endl;
      exit( EXIT_FAILURE );
    }
    wordTotal += word_count;
    lemTotal += lem_count;
    posTotal += pos_count;
#pragma omp critical
    {
      cout << "Processed :" << docName << " with " << word_count << " words,"
	   << " " << lem_count << " lemmas, and " << pos_count << " POS tags."
	   << " still " << --toDo << " files to go." << endl;
    }
    delete d;
  }

  if ( toDo > 1 ){
    cout << "done processsing directory '" << name << "' in total "
	 << wordTotal << " words were found." << endl;
  }

  if ( !hempName.empty() ){
    ofstream out( hempName );
    if ( out ){
      for( auto const& it : emph ){
	out << it << endl;
      }
      cout << "historical emphasis stored in: " << hempName << endl;
    }
    else {
      cerr << "unable to create historical emphasis file: " << hempName << endl;
    }
  }
  cout << "start calculating the results" << endl;
  string ext;
  if ( !lang.empty() && lang != "dut" ){
    ext += "." + lang;
  }
  if ( nG > 1 ){
    ext += "." + toString( nG ) + "-gram";
  }
  ext += ".tsv";
#pragma omp parallel sections
  {
#pragma omp section
    {
      string filename;
      filename = outputPrefix + ".wordfreqlist" + ext;
      create_wf_list( wc, filename, wordTotal, clip, dopercentage );
    }
#pragma omp section
    {
      if ( !( mode == S_IN_P || mode == S_IN_D ) ){
	string filename;
	filename = outputPrefix + ".lemmafreqlist" + ext;
	create_lf_list( lc, filename, lemTotal, clip, dopercentage );
      }
    }
#pragma omp section
    {
      if ( !( mode == S_IN_P || mode == S_IN_D ) ){
	string filename;
	filename = outputPrefix + ".lemmaposfreqlist" + ext;
	create_lpf_list( lpc, filename, posTotal, clip, dopercentage );
      }
    }
  }
}
