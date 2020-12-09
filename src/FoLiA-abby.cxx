/*
  Copyright (c) 2014 - 2020
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
#include <map>
#include <vector>
#include <stdexcept>
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

string setname = "FoLiA-abby-set";
string classname = "OCR";
const string processor_name= "FoLiA-abby";
string processor_id;

enum font_style { REGULAR=0, ITALIC=1, BOLD=2, SMALLCAPS=4 };

inline font_style operator~( font_style f ){
  return (font_style)( ~(int)f );
}

inline font_style operator&( font_style f1, font_style f2 ){
  return (font_style)((int)f1&(int)f2);
}

inline font_style& operator&=( font_style& f1, font_style f2 ){
  f1 = (f1 & f2);
  return f1;
}

inline font_style operator|( font_style f1, font_style f2 ){
  return (font_style) ((int)f1|(int)f2);
}

inline font_style& operator|=( font_style& f1, font_style f2 ){
  f1 = (f1 | f2);
  return f1;
}

font_style stringToMode( const string& s ){
  if ( s.empty() ){
    return REGULAR;
  }
  else if ( s == "italic" ){
    return ITALIC;
  }
  else if ( s == "bold" ) {
    return BOLD;
  }
  else if ( s == "smallcaps" ) {
    return SMALLCAPS;
  }
  else {
    cerr << "FoLiA-abby: unsupported Font-Style " << s << " (ignored)" << endl;
    return REGULAR;
  }
}

string toString( font_style fs ){
  if ( fs == REGULAR ){
    return "";
  }
  string result;
  if ( fs & ITALIC ){
    result += "italic|";
  }
  if ( fs & BOLD ){
    result += "bold|";
  }
  if ( fs & SMALLCAPS ){
    result += "smallcaps|";
  }
  result.pop_back();
  return result;
}

ostream& operator<<( ostream& os, const font_style& fs ){
  os << toString( fs );
  return os;
}
struct font_info {
  font_info():
    _fs(REGULAR)
  {};
  font_info( const string& ff, const font_style fs ):
    _ff(ff),
    _fs(fs)
  {};
  string _ff;
  font_style _fs;
};

ostream& operator<<( ostream& os, const font_info& fi ){
  os << fi._ff << " " << fi._fs;
  return os;
}

string get_line( xmlNode *line ){
  UnicodeString result;
  list<xmlNode*> variants = TiCC::FindNodes( line, "*:wordRecVariants" );
  if ( !variants.empty() ){
    if ( verbose ){
#pragma omp critical
      {
	cout << "\t\t\tfound " << variants.size() << " wordRecVariants nodes" << endl;
      }
    }
    for ( const auto& var : variants ){
      list<xmlNode*> recs = TiCC::FindNodes( var, "*:wordRecVariant" );
      if ( recs.empty() ){
	// hapens sometimes, just skip..
      }
      else {
	if ( verbose ){
#pragma omp critical
	  {
	    cout << "\t\t\t\tfound " << recs.size() << " wordRecVariant nodes" << endl;
	  }
	}
	list<xmlNode*> text = TiCC::FindNodes( recs.front(), "*:variantText" );
	if ( verbose ){
#pragma omp critical
	  {
	    cout << "\t\t\t\tfound " << text.size() << " text nodes" << endl;
	  }
	}
	UnicodeString bla = TiCC::UnicodeFromUTF8(TiCC::XmlContent(text.front()));
	if ( verbose ){
#pragma omp critical
	  {
	    cout << "\t\t\t\t\traw text: '" << bla << "'" << endl;
	  }
	}
	UnicodeString tmp;
	for ( int i=0; i < bla.length(); ++i ){
	  UChar c = bla[i];
	  switch ( c ){
	  case ' ':
	    // fallthrough
	  case '\t':
	    // fallthrough
	  case '\n':
	    // fallthrough
	  case '\r':
	    break;
	  default:
	    tmp += c;
	  }
	}
	if ( verbose ){
#pragma omp critical
	  {
	    cout << "\t\t\t\t\tintermediate text: '" << tmp << "'" << endl;
	  }
	}
	if ( tmp.endsWith( "¬" ) ){
	  tmp.remove(tmp.length()-1);
	}
	else if ( tmp.endsWith( "-" ) ){
	  tmp.remove(tmp.length()-1);
	}
	else if ( !tmp.endsWith( " " ) ){
	  tmp += " ";
	}
	if ( verbose ){
#pragma omp critical
	  {
	    cout << "\t\t\t\t\tfinal text: '" << tmp << "'" << endl;
	  }
	}
	result += tmp;
      }
    }
  }
  else {
    list<xmlNode*> chars = TiCC::FindNodes( line, "*:charParams" );
    if ( verbose ){
#pragma omp critical
      {
	cout << "\t\t\t\tfound " << chars.size() << " chars" << endl;
      }
    }
    for ( const auto& ch : chars ){
      result += TiCC::UnicodeFromUTF8(TiCC::XmlContent(ch));
    }
    if ( result.endsWith( "¬" ) ){
      result.remove(result.length()-1);
    }
    else if ( result.endsWith( "-" ) ){
      result.remove(result.length()-1);
    }
    else if ( result.endsWith( "\n" ) ){
      result.remove(result.length()-1);
      if ( !result.endsWith( " " ) ){
	result += " ";
      }
    }
    else if ( !result.endsWith( " " ) ){
      result += " ";
    }
  }
  if ( verbose ){
#pragma omp critical
    {
      cout << "Word text = '" << result << "'" << endl;
    }
  }
  return TiCC::UnicodeToUTF8(result);
}

void update_font_info( font_info& line_font,
		       xmlNode *node,
		       const map<string,font_info>& font_styles ){
  string style = TiCC::getAttribute( node, "style" );
  if ( !style.empty() ){
    line_font = font_styles.at( style );
  }
  string ff = TiCC::getAttribute( node, "ff" );
  if ( !ff.empty() ){
    line_font._ff = ff;
  }
  string bold = TiCC::getAttribute( node, "bold" );
  if ( !bold.empty() ){
    if ( bold == "1" ){
      line_font._fs |= BOLD;
    }
    else {
      line_font._fs &= ~BOLD;
    }
  }
  string italic = TiCC::getAttribute( node, "italic" );
  if ( !italic.empty() ){
    if ( italic == "1" ){
      line_font._fs |= ITALIC;
    }
    else {
      line_font._fs &= ~ITALIC;
    }
  }
  string small = TiCC::getAttribute( node, "smallcaps" );
  if ( !small.empty() ){
    if ( small == "1" ){
      line_font._fs |= SMALLCAPS;
    }
    else {
      line_font._fs &= ~SMALLCAPS;
    }
  }
}

void process_line( xmlNode *block,
		   map<string,vector<string>>& parts,
		   vector<pair<font_info,string>>& line_parts,
		   const font_info& default_font,
		   const map<string,font_info>& font_styles ){
  list<xmlNode*> formats = TiCC::FindNodes( block, "*:formatting" );
  if ( verbose ){
#pragma omp critical
    {
      cout << "\t\t\tfound " << formats.size() << " formatting nodes" << endl;
    }
  }

  for ( const auto& form : formats ){
    font_info line_font = default_font;
    update_font_info( line_font, form, font_styles );
    string result = get_line( form );
    if ( line_font._fs & SMALLCAPS ){
      parts["lemma"].push_back(result);
    }
    else {
      parts["entry"].push_back(result);
    }
    line_parts.push_back( make_pair( line_font, result ) );
  }
}

void output_result( const string& line,
		    const font_style& style,
		    folia::FoliaElement *root ){
  folia::KWargs args;
  args["processor"] = processor_id;
  root->doc()->declare( folia::AnnotationType::PART, setname, args );
  args.clear();
  //  args["xml:id"] = root->id() + ".entry";
  args["generate_id"] = root->id();
  folia::Part *part = new folia::Part( args, root->doc() );
  root->append( part );
  if ( style == REGULAR ){
    part->settext( line, classname );
  }
  else {
    folia::TextContent *tc = new folia::TextContent( root->doc() );
    part->append( tc );
    folia::KWargs args;
    args["processor"] = processor_id;
    root->doc()->declare( folia::AnnotationType::STYLE, setname, args );
    args.clear();
    args["class"] = toString( style );
    args["text"] = line;
    folia::TextMarkupStyle *st = new folia::TextMarkupStyle( args, root->doc() );
    tc->append( st );
  }
}

bool process_par( folia::FoliaElement *root,
		  xmlNode *par,
		  const map<string,font_info>& font_styles ){
  font_info par_font;
  string par_style = TiCC::getAttribute( par, "style" );
  if ( !par_style.empty() ){
    par_font = font_styles.at(par_style);
  }
  list<xmlNode*> lines = TiCC::FindNodes( par, "*:line" );
  if ( verbose ){
#pragma omp critical
    {
      cout << "\t\tfound " << lines.size() << " lines" << endl;
    }
  }
  map<string,vector<string>> parts;
  vector<pair<font_info,string>> line_parts;
  for ( const auto& line : lines ){
    process_line( line, parts, line_parts, par_font, font_styles );
  }
#ifdef OLD
  string lemma;
  string entry;
  for ( const auto& it : parts ){
    string result;
    for ( const auto& s : it.second ){
      result += s;
    }
    result = TiCC::trim(result);
    if ( it.first == "lemma" ){
      lemma += result;
    }
    else {
      entry += result;
    }
  }
  lemma = TiCC::trim(lemma);
  entry = TiCC::trim(entry);
  bool didit = false;
  if ( !lemma.empty() ){
    folia::KWargs args;
    args["processor"] = processor_id;
    root->doc()->declare( folia::AnnotationType::PART, setname, args );
    args.clear();
    args["xml:id"] = root->id() + ".lemma";
    folia::Part *part = new folia::Part( args );
    root->append( part );
    part->settext( lemma, classname );
    didit = true;
  }
  if ( !entry.empty() ){
    folia::KWargs args;
    args["processor"] = processor_id;
    root->doc()->declare( folia::AnnotationType::PART, setname, args );
    args.clear();
    args["xml:id"] = root->id() + ".entry";
    folia::Part *part = new folia::Part( args );
    root->append( part );
    part->settext( entry, classname );
    didit = true;
  }
#endif
  bool didit = false;
  string result;
  font_style current_style = REGULAR;
  for ( const auto& it : line_parts ){
    //    cerr << "part: " << it.second << " (" << it.first._fs << ")" << endl;
    if ( it.first._fs != current_style ){
      if ( !result.empty() ){
	output_result( result, current_style, root );
	result.clear();
      }
      current_style = it.first._fs;
    }
    if ( !it.second.empty() ){
      result += it.second;
    }
  }
  if ( !result.empty() ){
    output_result( result, current_style, root );
    didit = true;
  }
  return didit;
}

bool process_page( folia::FoliaElement *root,
		   xmlNode *block,
		   const map<string,font_info>& font_styles ){
  list<xmlNode*> paragraphs = TiCC::FindNodes( block, ".//*:par" );
  if ( verbose ){
#pragma omp critical
    {
      cout << "\tfound " << paragraphs.size() << " paragraphs" << endl;
    }
  }
  int i = 0;
  bool didit = false;
  for ( const auto& par_node : paragraphs ){
    folia::KWargs args;
    args["processor"] = processor_id;
    root->doc()->declare( folia::AnnotationType::PARAGRAPH, setname, args );
    args.clear();
    args["xml:id"] = root->id() + ".p" + TiCC::toString(++i);
    folia::Paragraph *paragraph = new folia::Paragraph( args, root->doc() );
    if ( process_par( paragraph, par_node, font_styles ) ){
      root->append( paragraph );
      didit = true;
    }
    else {
      --i;
      delete paragraph;
    }
  }
  return didit;
}

map<string,font_info> extract_font_info( xmlNode *root ){
  map<string,font_info> result;
  list<xmlNode*> par_styles =
    TiCC::FindNodes( root, ".//*:paragraphStyles/*:paragraphStyle" );
  //  cerr << "Found " << par_styles.size() << " paragraph style nodes" << endl;
  map<string,string> main_font_styles;
  for ( const auto& ps : par_styles ){
    string pid = TiCC::getAttribute( ps, "id" );
    string mf_id = TiCC::getAttribute( ps, "mainFontStyleId" );
    main_font_styles[pid] = mf_id;
    list<xmlNode*> font_styles =
      TiCC::FindNodes( ps, ".//*:fontStyle" );
    for ( const auto& fs : font_styles ){
      string id = TiCC::getAttribute( fs, "id" );
      string ff = TiCC::getAttribute( fs, "ff" );
      string it = TiCC::getAttribute( fs, "italic" );
      string bl = TiCC::getAttribute( fs, "bold" );
      font_style f_s = REGULAR;
      if ( it == "1" ){
	f_s = ITALIC;
      }
      else if ( bl == "1" ){
	f_s = BOLD;
      }
      font_info fi( ff, f_s );
      result.insert( make_pair(id,fi) );
    }
  }
  for ( const auto& mf : main_font_styles ){
    auto it = result.find(mf.second);
    if ( it != result.end() ){
      auto const val = it->second;
      result.insert( make_pair(mf.first,val) );
      //      cerr << "\t\tLINK" << mf.first << " ==> "<< mf.second << endl;
    }
    else {
      cerr << "surprise for " << mf.first << " ==> "<< mf.second << endl;
    }
  }
  return result;
}

bool convert_abbyxml( const string& fileName,
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
      cerr << "it MUST have extension .xml, .xml.bz2 or .xml.gz" << endl;
    }
    return false;
  }
  xmlNode *root = xmlDocGetRootElement( xdoc );
  list<xmlNode*> pages = TiCC::FindNodes( root, ".//*:page" );
  if ( pages.empty() ){
#pragma omp critical
    {
      cerr << "Problem finding pages node in " << fileName << endl;
    }
    return false;
  }
  map<string,font_info> font_styles = extract_font_info( root );

  // for ( const auto& fs : font_styles ){
  //   cerr << "fontstyle: " << fs.first << " " << fs.second << endl;
  // }

  string orgFile = TiCC::basename( fileName );
  string docid = orgFile.substr( 0, orgFile.find(".") );
  docid = prefix + docid;
  folia::Document doc( "xml:id='" + docid + "'" );
  doc.set_metadata( "abby_file", orgFile );
  folia::processor *proc = add_provenance( doc, processor_name, command );
  processor_id = proc->id();
  folia::KWargs args;
  args["xml:id"] =  docid + ".text";
  folia::Text *text = new folia::Text( args );
  doc.append( text );
  int i = 0;
  for ( const auto& page : pages ){
    folia::KWargs args;
    args["processor"] = processor_id;
    doc.declare( folia::AnnotationType::DIVISION, setname, args );
    args.clear();
    args["xml:id"] = text->id() + ".div" + TiCC::toString(++i);
    folia::Division *div = new folia::Division( args );
    text->append( div );
    process_page( div, page, font_styles );
  }

  string outName;
  if ( !outputDir.empty() ){
    outName = outputDir + "/";
  }
  string::size_type pos2 = orgFile.find( ".xml" );
  orgFile = orgFile.erase( pos2 );
  outName += orgFile + ".folia.xml";
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

  doc.save( outName );
  if ( verbose ){
#pragma omp critical
    {
      cout << "created " << outName << endl;
    }
  }
  return true;
}

void usage(){
  cerr << "Usage: FoLiA-abby [options] file/dir" << endl;
  cerr << "\t-t <threads>\n\t--threads <threads> Number of threads to run on." << endl;
  cerr << "\t\t\t If 'threads' has the value \"max\", the number of threads is set to a" << endl;
  cerr << "\t\t\t reasonable value. (OMP_NUM_TREADS - 2)" << endl;
  cerr << "\t-h or --help\t this messages " << endl;
  cerr << "\t-O\t\t output directory " << endl;
  cerr << "\t--setname='set'\t the FoLiA set name for <t> nodes. "
    "(default '" << setname << "')" << endl;
  cerr << "\t--class='class'\t the FoLiA class name for <t> nodes. "
    "(default '" << classname << "')" << endl;
  cerr << "\t--prefix='pre'\t add this prefix to the xml:id of ALL nodes in the created files. (default 'FA-') " << endl;
  cerr << "\t\t\t use 'none' for an empty prefix. (can be dangerous)" << endl;
  cerr << "\t--compress='c'\t with 'c'=b create bzip2 files (.bz2) " << endl;
  cerr << "\t\t\t with 'c'=g create gzip files (.gz)" << endl;
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
  if ( opts.extract( 'h' ) || opts.extract( "help" ) ){
    usage();
    exit(EXIT_SUCCESS);
  }
  if ( opts.extract( 'V' ) || opts.extract( "version" ) ){
    cerr << opts.prog_name() << " [" << PACKAGE_STRING << "]"<< endl;
    exit(EXIT_SUCCESS);
  }
  string orig_command = "FoLiA-abby " + opts.toString();
  if ( opts.extract( "compress", value ) ){
    if ( value == "b" ){
      outputType = BZ2;
    }
    else if ( value == "g" ) {
      outputType = GZ;
    }
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
    else {
      numThreads = TiCC::stringTo<int>( value );
    }
#else
    cerr << "OpenMP support is missing. -t options not supported!" << endl;
    exit( EXIT_FAILURE );
#endif
  }
  verbose = opts.extract( 'v' );
  opts.extract( 'O', outputDir );
  opts.extract( "setname", setname );
  opts.extract( "class", classname );
  string prefix = "FA-";
  opts.extract( "prefix", prefix );
  if ( prefix == "none" ){
    prefix.clear();
  }
  vector<string> fileNames = opts.getMassOpts();
  if ( fileNames.empty() ){
    cerr << "missing input file(s)" << endl;
    usage();
    exit(EXIT_FAILURE);
  }
  else if ( fileNames.size() > 1 ){
    cerr << "currently only 1 file or directory is supported" << endl;
    exit( EXIT_FAILURE );
  }

  if ( !outputDir.empty() ){
    string name = outputDir + "/";
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
    // we know name is a directory
    if ( name.back() == '/' ){
      name.pop_back();
    }

    fileNames = TiCC::searchFilesMatch( name, ".xml($|.gz$|.bz2$)", false );
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

#pragma omp parallel for shared(fileNames)
  for ( size_t fn=0; fn < fileNames.size(); ++fn ){
    if ( !convert_abbyxml( fileNames[fn],
			   outputDir,
			   outputType,
			   prefix,
			   orig_command ) )
#pragma omp critical
      {
	cerr << "failure on " << fileNames[fn] << endl;
      }
    else
#pragma omp critical
      {
	cout << "\tconverted " << fileNames[fn] << endl;
      }
  }
  cout << "done" << endl;
  exit(EXIT_SUCCESS);
}
