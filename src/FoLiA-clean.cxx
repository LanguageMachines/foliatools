/*
  Copyright (c) 2014 - 2022
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
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <fstream>

#include "ticcutils/CommandLine.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/Unicode.h"
#include "libfolia/folia.h"
#include "foliautils/common_code.h"

#include "config.h"
#ifdef HAVE_OPENMP
#include "omp.h"
#endif

using namespace	std;
using namespace	folia;

namespace std
{
  // needed to make unordered_[set|map] work on GCC < 5 (or for Clang
  // when uses old gcc includes)
  // This is a defect in de C++ standard and fixed in more recent compilers
  template<>
  class hash<folia::AnnotationType> {
  public:
    size_t operator()(const folia::AnnotationType &t) const
    {
      return static_cast<size_t>(t);
    }
  };
}


int debug = 0;

void clean_text( FoliaElement *node,
		 const string& textclass,
		 bool current ){
  if ( node->xmltag() == "w" ){
    // for Word nodes, we don't want te remove the last text present!
    vector<TextContent*> v = node->select<TextContent>();
    if ( v.size() <= 1 ){
      if ( debug ){
#pragma omp critical( debugging )
	{
	  cerr << "clean text: keep last text of " << node << endl;
	}
      }
      if ( current ){
	v[0]->set_to_current();
      }
      return;
    }
  }
  for ( size_t i=0; i < node->size(); ++i ){
    FoliaElement *p = node->index(i);
    if ( p->element_id() == TextContent_t ) {
      if ( !textclass.empty() ){
	if ( debug ){
#pragma omp critical( debugging )
	  {
	    cerr << "clean text: bekijk " << p << endl;
	    cerr << "p.text(" << p->cls() << ")" << endl;
	  }
	}
	if ( p->cls() != textclass ){
	  if ( debug ){
#pragma omp critical( debugging )
	    {
	      cerr << "different textclass: " <<  p->cls()
		   << "remove " << p << endl;
	    }
	  }
	  destroy( p );
	  --i;
	}
	else {
	  if ( debug ){
#pragma omp critical( debugging )
	    {
	      cerr << "same textclass: " <<  p->cls() << " keep" << endl;
	    }
	  }
	  if ( current ){
	    p->set_to_current();
	    if ( debug ){
#pragma omp critical( debugging )
	      {
		cerr << "set texclass to current: " << p << endl;
	      }
	    }
	  }
	  clean_text( p, textclass, current );
	}
      }
    }
    else {
      clean_text( p, textclass, current );
    }
  }
}

void clean_anno( FoliaElement *node,
		 const AnnotationType anno_type,
		 const string& setname ){
  for ( size_t i=0; i < node->size(); ++i ){
    FoliaElement *p = node->index(i);
    if ( debug ){
#pragma omp critical( debugging )
      {
	cerr << "clean anno: bekijk " << p << endl;
      }
    }
    string set = p->sett();
    AnnotationType at = p->annotation_type();
    if ( debug ){
#pragma omp critical( debugging )
      {
	cerr << "set = " << set << endl;
	cerr << "AT  = " << toString(at) << endl;
      }
    }
    if ( at == anno_type ){
      if ( debug ){
#pragma omp critical( debugging )
	{
	  cerr << "matched: " << toString(at) << "-annotation("
	       << set << ")" << endl;
	  cerr << "remove" << p << endl;
	}
      }
      if ( ( ( !set.empty() && setname == set )
	     || setname.empty() ) ){
	destroy( p );
	--i;
      }
    }
    else {
      clean_anno( p, anno_type, setname );
    }
  }
}

void clean_tokens( FoliaElement *node,
		   const string& textclass ){
  if ( node->xmltag() == "p"
       || node->xmltag() == "s" ){
    if ( debug ){
#pragma omp critical( debugging )
      {
	cerr << "clean tokens, bekijk "
	     << node << ", textclass='" << textclass << "'" << endl;
      }
    }
    UnicodeString s1;
    try {
      s1 = node->text(textclass,TEXT_FLAGS::STRICT );
    }
    catch (...){
    }
    if ( !s1.isEmpty() ){
      // So a STRICT text in the right class. We are done then
      return;
    }
    if ( debug ){
#pragma omp critical( debugging )
      {
	cerr << "S1 is leeg, dieper kijken " << endl;
      }
    }
    try {
      s1 = node->text( textclass ); // no retain tokenization, no strict
    }
    catch (...){
    }
    if ( debug ){
#pragma omp critical( debugging )
      {
	cerr << "S1= " << s1 << endl;
      }
    }
    if ( s1.isEmpty() ){
      // refuse to remove structure when in different textclass
      if ( debug ){
#pragma omp critical( debugging )
	{
	  cerr << "keep " << node << " with textclass=" << node->cls() << endl;
	  cerr << "met tekst: " <<  node->text( "current" ) << endl;
	}
      }
    }
    else {
      if ( debug ){
#pragma omp critical( debugging )
	{
	  cerr << "loop over " << node->xmltag() << " met "
	       << node->size() << " subnodes" << endl;
	}
      }
      for ( size_t i=0; i < node->size(); ++i ){
	FoliaElement *p = node->index(i);
	if ( debug ){
#pragma omp critical( debugging )
	  {
	    cerr << "bekijk " << p << endl;
	  }
	}
	if ( p->xmltag() == "w"
	     || p->xmltag() == "s" ){
	  if ( debug ){
#pragma omp critical( debugging )
	    {
	      cerr << "clean tokens: verwijder " << p << endl;
	    }
	  }
	  destroy( p );
	  --i;
	}
      }
      //    cerr << "na remove, node.size=" << node->size() << endl;
      if ( !s1.isEmpty() ) {
	if ( debug ){
#pragma omp critical( debugging )
	  {
	    cerr << "nu zet de tekst: " << s1 << " (" << textclass << ")"
		 << endl;
	  }
	}
	node->settext( TiCC::UnicodeToUTF8(s1), textclass );
      }
    }
  }
  else {
    if ( debug ){
#pragma omp critical( debugging )
      {
	cerr << "recurse clean tokens, bekijk " << node << endl;
      }
    }
    for ( size_t i=0; i < node->size(); ++i ){
      FoliaElement *p = node->index(i);
      clean_tokens( p, textclass );
    }
  }
}

void clean_doc( Document *d,
		const string& outname,
		const string& textclass,
		bool current,
		unordered_map< AnnotationType,
		unordered_set<string>>& anno_setname ){
  FoliaElement *root = d->doc();
  if ( anno_setname.find(AnnotationType::TOKEN) != anno_setname.end() ){
    // first clean token annotation
    clean_tokens( root, textclass );
    anno_setname.erase(AnnotationType::TOKEN);
    d->un_declare( AnnotationType::TOKEN, "" );
  }
  for( const auto& it : anno_setname ){
    for ( const auto& set : it.second ){
      clean_anno( root, it.first, set );
    }
  }
  for( const auto& it : anno_setname ){
    for ( const auto& set : it.second ){
      d->un_declare( it.first, set );
    }
  }
  if ( textclass != "current" ){
    clean_text( root, textclass, current );
  }
  d->save( outname );
}

void usage( const string& name ){
  cerr << "Usage: " << name << " [options] file/dir" << endl;
  cerr << "\t FoLiA-clean will produce a cleaned up version of a FoLiA file, " << endl;
  cerr << "\t or a whole directory of FoLiA files " << endl;
  cerr << "\t--textclass='name' retain only text nodes with this class. (default is to retain all)" << endl;
  cerr << "\t--current\t Make the textclass 'current'. (default is to keep 'name')" << endl;
  cerr << "\t--fixtext\t Fixup problems in structured text. " << endl;
  cerr << "\t--cleanannoset='type\\\\setname'\t remove annotations with 'type' and 'setname'. NOTE: use a double '\\' !. The setname can be empty. This option can be repeated for different annotations." << endl;
  cerr << "\t-e expr\t\t specify the expression all input files should match with. (default .folia.xml)" << endl;
  cerr << "\t--debug\t\t Set dubugging" << endl;
  cerr << "\t-t <threads>\n\t--threads <threads> Number of threads to run on." << endl;
  cerr << "\t\t\t If 'threads' has the value \"max\", the number of threads is set to a" << endl;
  cerr << "\t\t\t reasonable value. (OMP_NUM_TREADS - 2)" << endl;
  cerr << "\t-h or --help\t this message" << endl;
  cerr << "\t-V or --version show version " << endl;
  cerr << "\t-O\t\t name of the output dir." << endl;
}

int main( int argc, char *argv[] ){
  TiCC::CL_Options opts( "hVvpe:t:O:", "textclass:,current,cleanannoset:,help,version,fixtext,debug,threads:" );
  try {
    opts.init(argc,argv);
  }
  catch( TiCC::OptionError& e ){
    cerr << e.what() << endl;
    usage(argv[0]);
    exit( EXIT_FAILURE );
  }
  string progname = opts.prog_name();
  if ( opts.empty() ){
    usage( progname );
    exit(EXIT_FAILURE);
  }
  string value;
  if ( opts.extract('V') || opts.extract("version") ){
    cerr << PACKAGE_STRING << endl;
    exit(EXIT_SUCCESS);
  }
  if ( opts.extract('h') || opts.extract("help") ){
    usage(progname);
    exit(EXIT_SUCCESS);
  }
  string command = "FoLiA-clean " + opts.toString();
  string expression = ".folia.xml";
  opts.extract( 'e', expression );
  string output_dir;
  opts.extract( 'O', output_dir );
  bool make_current = opts.extract( "current" );
  if ( opts.extract( 't', value )
       || opts.extract( "threads", value ) ){
#ifdef HAVE_OPENMP
    int numThreads = 1;
    if ( TiCC::lowercase(value) == "max" ){
      numThreads = omp_get_max_threads() - 2;
    }
    else if ( !TiCC::stringTo(value,numThreads) ) {
      cerr << "illegal value for -t (" << value << ")" << endl;
      exit( EXIT_FAILURE );
    }
    omp_set_num_threads( numThreads );
#else
    cerr << "-t option does not work, no OpenMP support in your compiler?" << endl;
    exit( EXIT_FAILURE );
#endif
  }
  string class_name = "current";
  opts.extract( "textclass", class_name );
  if ( opts.extract( "debug" ) ){
    debug = 1;
  }
  bool fixtext = opts.extract( "fixtext" );
  if ( fixtext && make_current ){
    cerr << "cannot combine --fixtext and --current" << endl;
    exit( EXIT_FAILURE );
  }
  unordered_map<AnnotationType,
		unordered_set<string>> clean_sets;
  string line;
  while ( opts.extract( "cleanannoset", line ) ){
    string type;
    string setname;
    string::size_type bs_pos = line.find( "\\" );
    if ( bs_pos == string::npos ){
      // no '\' separator, so only a type
      type = line;
    }
    else {
      type = line.substr( 0, bs_pos );
      setname = line.substr( bs_pos+1 );
    }
    AnnotationType at;
    try {
      at = TiCC::stringTo<AnnotationType>( type );
    }
    catch ( exception& e){
      cerr << e.what() << endl;
      exit(EXIT_FAILURE);
    }
    clean_sets[at].insert(setname);
  }

  vector<string> fileNames = opts.getMassOpts();
  if ( fileNames.empty() ){
    cerr << "no file or dir specified!" << endl;
    exit(EXIT_FAILURE);
  }

  if ( fileNames.size() == 1 && TiCC::isDir( fileNames[0] ) ){
    fileNames = TiCC::searchFilesMatch( fileNames[0], expression );
  }
  size_t toDo = fileNames.size();
  if ( toDo == 0 ){
    cerr << "no matching files found" << endl;
    exit(EXIT_SUCCESS);
  }

  if ( !output_dir.empty() ){
    string::size_type pos = output_dir.find( "/" );
    if ( pos != string::npos && pos == output_dir.length()-1 ){
      // outputname ends with a /
    }
    else {
      output_dir += "/";
    }
    if ( !TiCC::createPath( output_dir ) ){
      cerr << "Output to '" << output_dir << "' is impossible" << endl;
    }
  }

  if ( toDo > 1 ){
    cout << "start processing of " << toDo << " files " << endl;
  }

#pragma omp parallel for shared(fileNames) schedule(dynamic)
  for ( size_t fn=0; fn < fileNames.size(); ++fn ){
    string docName = fileNames[fn];
    Document *d = 0;
    try {
      if ( fixtext ){
	d = new Document( "file='"+ docName + "',mode='fixtext'" );
      }
      else {
	d = new Document( "file='"+ docName + "'" );
      }
    }
    catch ( exception& e ){
#pragma omp critical
      {
	cerr << "failed to load document '" << docName << "'" << endl;
	cerr << "reason: " << e.what() << endl;
      }
      continue;
    }
    add_provenance( *d, "FoLia-clean", command );
    string outname ;
    string::size_type pos = docName.find( expression );
    if ( pos != string::npos ){
      outname = docName.substr( 0, pos );
    }
    else {
      outname = docName;
    }
    outname = output_dir + outname + ".cleaned.folia.xml";
    if ( !TiCC::createPath( outname ) ){
#pragma omp critical
      {
	cerr << "Output to '" << outname << "' is impossible" << endl;
      }
    }
    else {
      clean_doc( d, outname, class_name, make_current, clean_sets );
#pragma omp critical
      {
	cout << "Processed :" << docName << " into " << outname
	     << " still " << --toDo << " files to go." << endl;
      }
    }
    delete d;
  }
  return EXIT_SUCCESS;
}
