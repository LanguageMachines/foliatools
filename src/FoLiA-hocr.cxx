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
#include <list>
#include <vector>
#include <iostream>
#include <fstream>
#include "ticcutils/StringOps.h"
#include "libfolia/folia.h"
#include "ticcutils/XMLtools.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/zipper.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/Unicode.h"
#include "ticcutils/CommandLine.h"
#include "foliautils/common_code.h"
#include "config.h"
#ifdef HAVE_OPENMP
#include "omp.h"
#endif

using namespace	std;
using namespace	icu;

bool verbose = false;
string setname = "FoLiA-hocr-set";
string classname = "OCR";
string processor_id;

string extractContent( xmlNode* pnt ) {
  string result;
  if ( pnt ){
    result = TiCC::XmlContent(pnt);
    if ( result == "" )
      return extractContent( pnt->children );
  }
  return result;
}


void processParagraphs( xmlNode *div, folia::FoliaElement *out, const string& file ){
  list<xmlNode*> pars = TiCC::FindNodes( div, "//p" );
  if ( verbose ){
#pragma omp critical
    {
      cout << "processParagraphs: number=" << pars.size() << endl;
    }
  }
  for ( const auto& p : pars ){
    list<xmlNode*> lines = TiCC::FindNodes( p, ".//span[@class='ocr_line']" );
    if ( lines.empty() ){
#pragma omp critical
      {
	cerr << "found no 'ocr_line' nodes in paragraph id="
	     << TiCC::getAttribute( p, "id" ) << " (continuing)" << endl;
      }
      continue;
    }
    if ( verbose ){
#pragma omp critical
      {
	cout << "\thandling 'ocr_line' total=" << lines.size() << endl;
      }
    }
    string p_id = TiCC::getAttribute( p, "id" );
    folia::KWargs p_args;
    p_args["processor"] = processor_id;
    out->doc()->declare( folia::AnnotationType::PARAGRAPH, setname,  p_args );
    p_args["xml:id"] = out->id() + "." + p_id;
    folia::Paragraph *par = out->add_child<folia::Paragraph>( p_args );
    UnicodeString txt;
    for ( const auto& line : lines ){
      list<xmlNode*> words = TiCC::FindNodes( line,
					      ".//span[@class='ocrx_word']" );
      if ( words.empty() ){
	// no ocrx_words. Lets see...
	words = TiCC::FindNodes( line, ".//span[@class='ocr_word']" );
	if ( words.empty() ){
#pragma omp critical
	  {
	    cerr << "found no OCRX_WORD or OCR_WORD nodes in " << file << endl;
	  }
	  destroy( par );
	  // maybe just warn and skip to next line?
	  return;
	}
      }
      if ( verbose ){
#pragma omp critical
	{
	  cout << "\t\thandling 'ocr_word' total=" << words.size() << endl;
	}
      }
      for ( const auto& word : words ){
	string w_id = TiCC::getAttribute( word, "id" );
	string content = extractContent( word );
	content = TiCC::trim( content );
	if ( !content.empty() ){
	  folia::KWargs args;
	  args["xml:id"] = par->id() + "." + w_id;
	  folia::String *str = par->add_child<folia::String>( args );
	  UnicodeString uc = TiCC::UnicodeFromUTF8( content );
	  str->setutext( uc, txt.length(), classname );
	  txt += " " + uc;
	  args.clear();
	  args["processor"] = processor_id;
	  out->doc()->declare( folia::AnnotationType::RELATION,
			       setname,  args );
	  args.clear();
	  args["xlink:href"] = file;
	  args["format"] = "text/hocr+xml";
	  folia::Relation *h = str->add_child<folia::Relation>( args );
	  args.clear();
	  args["id"] = w_id;
	  args["type"] = "str";
	  h->add_child<folia::LinkReference>( args );
	}
      }
    }
    if ( txt.length() > 1 ){
      par->setutext( txt.tempSubString(1), classname );
    }
    else {
      destroy( par );
    }
  }
}

string getDocId( const string& title ){
  string result;
  vector<string> vec = TiCC::split_at( title, ";" );
  for ( const auto& part : vec ){
    vector<string> v2 = TiCC::split( part );
    if ( v2.size() == 2 ){
      if ( TiCC::trim( v2[0] ) == "image" ){
	result = v2[1];
	string::size_type pos = result.rfind( "/" );
	if ( pos != string::npos ){
	  result = result.substr( pos+1 );
	}
      }
    }
  }
  result = TiCC::trim( result, " \t\"" );
  return result;
}

void convert_hocr( const string& fileName,
		   const string& outputDir,
		   const zipType outputType,
		   const string& prefix,
		   const string& command ){
  if ( verbose ){
#pragma omp critical
    {
      cout << "start handling " << fileName << endl;
    }
  }
  zipType inputType;
  xmlDoc *xdoc = getXml( fileName, inputType );
  if ( !xdoc ){
#pragma omp critical
    {
      cerr << "problem detecting type of file: " << fileName << endl;
      cerr << "it MUST have extension .hocr, .html or .xhtml (or .bz2 or .gz variants)" << endl;
    }
    return;
  }
  xmlNode *root = xmlDocGetRootElement( xdoc );
  list<xmlNode*> divs = TiCC::FindNodes( root, "//div[@class='ocr_page']" );
  if ( divs.empty() ) {
#pragma omp critical
    {
      cerr << "no OCR_PAGE node found in " << fileName << endl;
    }
    exit(EXIT_FAILURE);
  }
  if ( divs.size() > 1 ) {
#pragma omp critical
    {
      cerr << "multiple OCR_PAGE nodes found. Not supported. in " << fileName << endl;
    }
    exit(EXIT_FAILURE);
  }
  string title = TiCC::getAttribute( *divs.begin(), "title" );
  if ( title.empty() ){
#pragma omp critical
    {
      cerr << "No 'title' attribute found in ocr_page: " << fileName << endl;
    }
    exit(EXIT_FAILURE);
  }
  title = getDocId( title );
  string docid = title.substr( 0, title.find(".") );
  docid = prefix + docid;
  folia::Document doc( "xml:id='" + docid + "'" );
  doc.set_metadata( "filename", title );
  folia::processor *proc = add_provenance( doc, "FoLiA-hocr", command );
  processor_id = proc->id();
  folia::KWargs args;
  args["processor"] = processor_id;
  doc.declare( folia::AnnotationType::STRING, setname,  args );
  args.clear();
  args["xml:id"] = docid + ".text";
  folia::Text *text = doc.create_root<folia::Text>( args );
  processParagraphs( root, text, TiCC::basename(fileName) );
  xmlFreeDoc( xdoc );

  string outName = outputDir + "/" + prefix + title + ".folia.xml";
  zipType type = inputType;
  if ( outputType != NORMAL ){
    type = outputType;
  }
  if ( type == BZ2 ){
    outName += ".bz2";
  }
  else if ( type == GZ ){
    outName += ".gz";
  }
  vector<folia::Paragraph*> pv = doc.paragraphs();
  if ( pv.size() == 0 ||
       ( pv.size() == 1 && pv[0]->size() == 0 ) ){
    // no paragraphs, or just 1 without data
#pragma omp critical
    {
      cerr << "skipped empty result : " << outName << endl;
    }
  }
  else {
    doc.save( outName );
    if ( verbose ){
#pragma omp critical
      {
	cout << "created " << outName << endl;
      }
    }
  }
}

void usage(){
  cerr << "Usage: FoLiA-hocr [options] file/dir" << endl;
  cerr << "\t-t 'threads' or\n\t--threads='threads' Number of threads to run on." << endl;
  cerr << "\t\t\t If 'threads' has the value \"max\", the number of threads is set to a" << endl;
  cerr << "\t\t\t reasonable value. (OMP_NUM_TREADS - 2)" << endl;
  cerr << "\t-h or --help\t this message " << endl;
  cerr << "\t-O\t\t output directory " << endl;
  cerr << "\t--compress='c'\t with 'c'=b create bzip2 files (.bz2) " << endl;
  cerr << "\t\t\t with 'c'=g create gzip files (.gz)" << endl;
  cerr << "\t--setname='set'\t the FoLiA set name for <t> nodes. "
    "(default '" << setname << "')" << endl;
  cerr << "\t--class='class'\t the FoLiA class name for <t> nodes. "
    "(default '" << classname << "')" << endl;
  cerr << "\t--prefix='pre'\t add this prefix to ALL created files. (default 'FH-') " << endl;
  cerr << "\t\t\t use 'none' for an empty prefix. (can be dangerous)" << endl;
  cerr << "\t-v\t\t verbose output " << endl;
  cerr << "\t-V or --version\t show version " << endl;
}

int main( int argc, char *argv[] ){
  TiCC::CL_Options opts( "vVt:O:h",
			 "compress:,class:,setname:,help,version,prefix:,threads:" );
  try {
    opts.init( argc, argv );
  }
  catch( TiCC::OptionError& e ){
    cerr << e.what() << endl;
    usage();
    exit( EXIT_FAILURE );
  }
#ifdef HAVE_OPENMP
  int numThreads=1;
#endif

  string outputDir;
  zipType outputType = NORMAL;
  string value;
  if ( opts.empty() ){
    usage();
    exit(EXIT_FAILURE);
  }
  if ( opts.extract( 'h' ) || opts.extract( "help" ) ){
    usage();
    exit(EXIT_SUCCESS);
  }
  if ( opts.extract( 'V' ) || opts.extract( "version" ) ){
    cerr << PACKAGE_STRING << endl;
    exit(EXIT_SUCCESS);
  }
  string orig_command = "FoLiA-hocr " + opts.toString();
  verbose = opts.extract( 'v', value );
  if ( opts.extract( "compress", value ) ){
    if ( value == "b" )
      outputType = BZ2;
    else if ( value == "g" )
      outputType = GZ;
    else {
      cerr << "unknown compression: use 'b' or 'g'" << endl;
      exit( EXIT_FAILURE );
    }
  }
  if ( opts.extract( 't', value )
       || opts.extract( "threads", value ) ){
#ifdef HAVE_OPENMP
    if ( TiCC::lowercase(value) == "max" ){
      numThreads = omp_get_max_threads() - 2;
    }
    else if ( !TiCC::stringTo(value,numThreads) ) {
      cerr << "illegal value for -t (" << value << ")" << endl;
      exit( EXIT_FAILURE );
    }
#else
    cerr << "You don't have OpenMP support, so '-t' option is impossible" << endl;
    exit(EXIT_FAILURE);
#endif
  }
  opts.extract( 'O', outputDir );
  opts.extract( "setname", setname );
  opts.extract( "class", classname );
  string prefix = "FH-";
  opts.extract( "prefix", prefix );
  if ( prefix == "none" ){
    prefix.clear();
  }
  vector<string> fileNames = opts.getMassOpts();
  if ( fileNames.empty() ){
    cerr << "missing input file(s)" << endl;
    exit(EXIT_FAILURE);
  }
  else if ( fileNames.size() > 1 ){
    cerr << "currently only 1 file or directory is supported" << endl;
    exit( EXIT_FAILURE );
  }

  if ( !outputDir.empty() ){
    string name = outputDir;
    if ( !TiCC::isDir(name) ){
      if ( !TiCC::createPath( name ) ){
	cerr << "outputdir '" << name
	     << "' doesn't exist and can't be created" << endl;
	exit(EXIT_FAILURE);
      }
    }
  }
  string name = fileNames[0];
  if ( !( TiCC::isFile(name) || TiCC::isDir(name) ) ){
    cerr << "parameter '" << name << "' doesn't seem to be a file or directory"
	 << endl;
    exit(EXIT_FAILURE);
  }
  if ( TiCC::isFile(name) ){
    if ( TiCC::match_back( name, ".tar" ) ){
      cerr << "TAR files are not supported yet." << endl;
      exit(EXIT_FAILURE);
    }
  }
  else {
    fileNames = TiCC::searchFilesMatch( name, "*hocr" );
    if ( fileNames.empty() ){
      fileNames = TiCC::searchFilesMatch( name, "*html" );
    }
  }
  size_t toDo = fileNames.size();
  if ( toDo == 0 ){
    cerr << "no matching files found." << endl;
    exit(EXIT_FAILURE);
  }
  if ( toDo > 1 ){
    cout << "start processing of " << toDo << " files " << endl;
  }

#ifdef HAVE_OPENMP
  if ( numThreads >= 1 ){
    omp_set_num_threads( numThreads );
  }
#endif

#pragma omp parallel for shared(fileNames) schedule(dynamic)
  for ( size_t fn=0; fn < fileNames.size(); ++fn ){
    convert_hocr( fileNames[fn], outputDir, outputType, prefix, orig_command );
  }
  cout << "done" << endl;
  exit(EXIT_SUCCESS);
}
