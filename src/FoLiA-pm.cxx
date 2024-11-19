/*
  Copyright (c) 2014 - 2024
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
#include <cassert>
#include "ticcutils/StringOps.h"
#include "libfolia/folia.h"
#include "ticcutils/XMLtools.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/zipper.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/FileUtils.h"
#include "foliautils/common_code.h"
#include "config.h"
#ifdef HAVE_OPENMP
#include "omp.h"
#endif

using namespace	std;
using namespace	folia;
using TiCC::operator<<;

bool verbose = false;
string setname = "polmash";
string processor_id;

KWargs getAllAttributes( const xmlNode *node ){
  /// unless folia::getAttribues, this gathers ALL attributes
  /// regardless of namespace, xml:id or xlink: cases
  KWargs atts;
  if ( node ){
    xmlAttr *a = node->properties;
    while ( a ){
      atts[folia::to_string(a->name)] = TiCC::TextValue(a->children);
      a = a->next;
    }
  }
  return atts;
}

void process_stage( Division *, xmlNode * );

string extract_embedded( xmlNode *p ){
  string result;
  if ( p == 0 ){
    return result;
  }
  if ( verbose ){
#pragma omp critical
    {
      cerr << "extract embedded: " << TiCC::Name(p) << endl;
    }
  }
  p = p->children;
  while ( p ){
    if ( verbose ){
#pragma omp critical
      {
	cerr << "examine: " << TiCC::Name(p) << endl;
      }
    }
    string content = TiCC::TextValue(p);
    if ( !content.empty() ){
      if ( verbose ){
#pragma omp critical
	{
	  cerr << "embedded add: " << content << endl;
	}
      }
      result += content;
    }
    else {
      string part = extract_embedded( p->children );
      result += part;
    }
    p = p->next;
  }
  return result;
}

void add_reference( TextContent *tc, const xmlNode *p ){
  if ( verbose ){
#pragma omp critical
    {
      cerr << "add_reference" << endl;
    }
  }
  string text_part;
  string ref;
  string sub_type;
  string status;
  xmlNode *t = p->children;
  while ( t ){
    if ( t->type == XML_TEXT_NODE ){
      xmlChar *tmp = xmlNodeGetContent( t );
      if ( tmp ){
	text_part = folia::to_string( tmp );
	xmlFree( tmp );
      }
    }
    else if ( TiCC::Name(t) == "tagged-entity" ){
      ref = TiCC::getAttribute( t, "reference" );
      sub_type = TiCC::getAttribute( t, "sub-type" );
      status = TiCC::getAttribute( t, "status" );
    }
    else {
#pragma omp critical
      {
	cerr << "reference unhandled: "
	     << TiCC::Name(t) << endl;
      }
    }
    t = t->next;
  }
  if ( ref.empty() && !text_part.empty() ){
    ref = "unknown";
  }
  if ( !ref.empty() ){
    KWargs args;
    args["xlink:href"] = ref;
    args["xlink:type"] = "locator";
    if ( !sub_type.empty() ){
      args["xlink:role"] = sub_type;
    }
    if ( !status.empty() ){
      args["xlink:label"] = status;
    }
    if ( !text_part.empty() ){
      args["text"] = text_part;
    }
    tc->add_child<TextMarkupString>( args );
  }
}

void add_note( Note *root, xmlNode *p ){
  //  cerr << "add note: " << TiCC::getAttributes( p ) << endl;
  string id = TiCC::getAttribute( p, "id" );
  if ( verbose ){
#pragma omp critical
    {
      cerr << "add_note: id=" << id << endl;
    }
  }
  KWargs args;
  args["processor"] = processor_id;
  root->doc()->declare( folia::AnnotationType::PARAGRAPH, setname, args );
  args["xml:id"] = id;
  Paragraph *par = root->add_child<Paragraph>( args );
  TextContent *tc = par->add_child<TextContent>();
  p = p->children;
  while ( p ){
    if ( p->type == XML_TEXT_NODE ){
      xmlChar *tmp = xmlNodeGetContent( p );
      if ( tmp ){
	string val = folia::to_string( tmp );
	tc->add_child<XmlText>( val );
	xmlFree( tmp );
      }
    }
    else if ( p->type == XML_ELEMENT_NODE ){
      string tag = TiCC::Name( p );
      if ( tag == "tagged" ){
	string tag_type = TiCC::getAttribute( p, "type" );
	if ( tag_type == "reference" ){
	  add_reference( tc, p );
	}
	else {
#pragma omp critical
	  {
	    cerr << "tagged: " << id << ", unhandled type=" << tag_type << endl;
	  }
	}
      }
      else {
#pragma omp critical
	{
	  cerr << "note: " << id << ", unhandled tag : " << tag << endl;
	}
      }
    }
    p = p->next;
  }
}

void add_entity( FoliaElement* root, xmlNode *p ){
  if ( verbose ){
#pragma omp critical
    {
      cerr << "add_entity "<< endl;
    }
  }
  string text_part;
  string mem_ref;
  string part_ref;
  string id = TiCC::getAttribute( p, "id" );
  xmlNode *t = p->children;
  while ( t ){
    if ( t->type == XML_TEXT_NODE ){
      xmlChar *tmp = xmlNodeGetContent( t );
      if ( tmp ){
	text_part = folia::to_string( tmp );
	xmlFree( tmp );
      }
    }
    else if ( TiCC::Name(t) == "tagged-entity" ){
      mem_ref = TiCC::getAttribute( t, "member-ref" );
      part_ref = TiCC::getAttribute( t, "party-ref" );
    }
    else {
#pragma omp critical
      {
	cerr << "entity" << id << ", unhandled: " << TiCC::Name(t) << endl;
      }
    }
    t = t->next;
  }
  KWargs args;
  args["processor"] =  processor_id;
  root->doc()->declare( folia::AnnotationType::ENTITY,
			setname, args );
  EntitiesLayer *el = root->add_child<EntitiesLayer>( args );
  args["class"] = "member";
  Entity *ent = el->add_child<Entity>( args );
  args.clear();
  args["subset"] = "member-ref";
  if ( !mem_ref.empty() ){
    args["class"] = mem_ref;
  }
  else {
    args["class"] = "unknown";
  }
  ent->add_child<Feature>( args );
  args.clear();
  args["subset"] = "party-ref";
  if ( !part_ref.empty() ){
    args["class"] = part_ref;
  }
  else {
    args["class"] = "unknown";
  }
  ent->add_child<Feature>( args );
  args.clear();
  args["subset"] = "name";
  if ( !text_part.empty() ){
    args["class"] = text_part;
  }
  else {
    args["class"] = "unknown";
  }
  ent->add_child<Feature>( args );
}

void add_stage_direction( TextContent *tc, xmlNode *p ){
  string embedded = extract_embedded( p );
  string id = TiCC::getAttribute( p, "id" );
  if ( embedded.empty() ){
#pragma omp critical
    {
      cerr << "stage-direction: " << id << " without text?"
	   << endl;
    }
  }
  else {
    if ( p->next != 0 ){
      // not last
      embedded += " ";
    }
    tc->add_child<XmlText>( embedded );
  }
}

void add_notes( FoliaElement *par, const list<Note*>& notes ){
  KWargs args;
  args["xml:id"] = par->id() + ".d.1";
  args["class"] = "notes";
  args["processor"] = processor_id;
  Division *div = par->parent()->add_child<Division>( args );
  for ( const auto& note : notes ){
    div->append( note );
  }
}

Paragraph *add_par( Division *root, xmlNode *p, list<Note*>& notes ){
  string par_id = TiCC::getAttribute( p, "id" );
  if ( verbose ){
#pragma omp critical
    {
      cerr << "add_par: id=" << par_id << endl;
    }
  }
  folia::Document *doc = root->doc();
  KWargs par_args;
  par_args["processor"] = processor_id;
  doc->declare( folia::AnnotationType::PARAGRAPH, setname, par_args );
  par_args["xml:id"] = par_id;
  Paragraph *par = root->add_child<Paragraph>( par_args );
  TextContent *tc = par->add_child<TextContent>();
  p = p->children;
  while ( p ){
    if ( p->type == XML_TEXT_NODE ){
      xmlChar *tmp = xmlNodeGetContent( p );
      if ( tmp ){
	string part = folia::to_string( tmp );
	tc->add_child<XmlText>( part );
	xmlFree( tmp );
      }
    }
    else if ( p->type == XML_ELEMENT_NODE ){
      string tag = TiCC::Name( p );
      if ( tag == "tagged" ){
	string tag_type = TiCC::getAttribute( p, "type" );
	if ( tag_type == "reference" ){
	  add_reference( tc, p );
	}
	else if ( tag_type == "named-entity" ) {
	  add_entity( root, p );
	}
      }
      else if ( tag == "stage-direction" ){
	add_stage_direction( tc, p );
      }
      else if ( tag == "note" ){
	string id = TiCC::getAttribute( p, "id" );
	string ref = TiCC::getAttribute( p, "ref" );
	string number;
	string text;
	xmlNode *pp = p->children;
	while ( pp ){
	  if ( number.empty() ){
	    number = TiCC::TextValue( pp );
	  }
	  else {
	    text = TiCC::TextValue( pp );
	  }
	  pp=pp->next;
	}
	if ( number.empty() || text.empty() ){
#pragma omp critical
	  {
	    cerr << "problem with note id= " << id
		 << " No index or text found? " << endl;
	  }
	}
	else {
	  tc->add_child<XmlText>( " " );
	  KWargs args;
	  args["xml:id"] = id;
	  args["id"] = ref;
	  args["type"] = "note";
	  args["text"] = number;
	  tc->add_child<TextMarkupReference>( args );
	  tc->add_child<XmlText>( " " );
	}
	if ( verbose ){
#pragma omp critical
	  {
	    cerr << "paragraph:note: " << id << ", ref= " << ref << endl;
	  }
	}
	Note *note = 0;
	if ( ref.empty() ){
	  KWargs args;
	  args["xml:id"] = id;
	  note = new Note( args, doc );
	}
	else {
	  ref = create_NCName( ref );
	  KWargs args;
	  args["xml:id"] = ref;
	  note = new Note( args, doc );
	}
	notes.push_back( note );
	xmlNode *pnt = p->children;
	while ( pnt ){
	  tag = TiCC::Name(pnt);
	  if ( tag == "p" ){
	    add_note( note, pnt );
	  }
	  else {
#pragma omp critical
	    {
	      cerr << "note: " << id << ", unhandled tag : " << tag << endl;
	    }
	  }
	  pnt = pnt->next;
	}
      }
      else {
#pragma omp critical
	{
	  cerr << "paragraph: " << par_id << ", unhandled tag : " << tag << endl;
	}
      }
    }
    p = p->next;
  }
  return par;
}

void process_chair( Division *root, const xmlNode *chair ){
  if ( verbose ){
#pragma omp critical
    {
      cerr << "process_chair" << endl;
    }
  }
  xmlNode *p = chair->children;
  while ( p ){
    string label = TiCC::Name(p);
    if ( label == "p" ){
      list<Note*> notes;
      Paragraph *par = add_par( root, p, notes );
      if ( !notes.empty() ){
	add_notes( par, notes );
      }
    }
    else if ( label == "chair" ){
      string speaker = TiCC::getAttribute( p, "speaker" );
      if ( speaker.empty() ){
	speaker = "unknown";
      }
      string member = TiCC::getAttribute( p, "member-ref" );
      if ( member.empty() ){
	member = "unknown";
      }
      KWargs args;
      args["subset"] = "speaker";
      args["class"] = speaker;
      root->add_child<Feature>( args );
      args["subset"] = "member-ref";
      args["class"] = member;
      root->add_child<Feature>( args );
    }
    else {
#pragma omp critical
      {
	cerr << "chair, unhandled: " << label << endl;
      }
    }
    p = p->next;
  }
}

void add_entity( EntitiesLayer *root, xmlNode *p ){
  string id = TiCC::getAttribute( p, "id" );
  if ( verbose ){
#pragma omp critical
    {
      cerr << "add_entity: id=" << id << endl;
    }
  }
  KWargs args;
  p = p->children;
  while ( p ){
    string tag = TiCC::Name( p );
    if ( tag == "tagged" ){
      string tag_type = TiCC::getAttribute( p, "type" );
      if ( tag_type == "named-entity" ) {
	if ( verbose ){
#pragma omp critical
	  {
	    cerr << "add_entity: " << tag_type << endl;
	  }
	}
	string text_part;
	string mem_ref;
	string part_ref;
	xmlNode *t = p->children;
	while ( t ){
	  if ( t->type == XML_TEXT_NODE ){
	    xmlChar *tmp = xmlNodeGetContent( t );
	    if ( tmp ){
	      text_part = folia::to_string( tmp );
	      xmlFree( tmp );
	    }
	  }
	  else if ( TiCC::Name(t) == "tagged-entity" ){
	    mem_ref = TiCC::getAttribute( t, "member-ref" );
	    part_ref = TiCC::getAttribute( t, "party-ref" );
	  }
	  else {
#pragma omp critical
	    {
	      cerr << "tagged" << id << ", unhandled: "
		   << TiCC::Name(t) << endl;
	    }
	  }
	  t = t->next;
	}
	args.clear();
	args["class"] = "member";
	args["xml:id"] = id;
	Entity *ent = root->add_child<Entity>( args );
	args.clear();
	args["subset"] = "member-ref";
	if ( !mem_ref.empty() ){
	  args["class"] = mem_ref;
	}
	else {
	  args["class"] = "unknown";
	}
	ent->add_child<Feature>( args );
	args.clear();
	args["subset"] = "party-ref";
	if ( !part_ref.empty() ){
	  args["class"] = part_ref;
	}
	else {
	  args["class"] = "unknown";
	}
	ent->add_child<Feature>( args );
	args.clear();
	args["subset"] = "name";
	if ( !text_part.empty() ){
	  args["class"] = text_part;
	}
	else {
	  args["class"] = "unknown";
	}
	ent->add_child<Feature>( args );
      }
    }
    else {
#pragma omp critical
      {
	cerr << "add_entity: " << id << ", unhandled tag : " << tag << endl;
      }
    }
    p = p->next;
  }
}

void process_speech( Division *root, xmlNode *speech ){
  KWargs atts = getAllAttributes( speech );
  string id = atts["id"];
  if ( verbose ){
#pragma omp critical
    {
      using TiCC::operator<<;
      cerr << "process_speech: atts" << atts << endl;
    }
  }
  string type = atts["type"];
  KWargs d_args;
  d_args["xml:id"] = id;
  d_args["class"] = type;
  d_args["processor"] = processor_id;
  Division *div = root->add_child<Division>( d_args );
  for ( const auto& [att,cls] : atts ){
    if ( att == "id"
	 || att == "type" ){
      continue;
    }
    else if ( att == "speaker"
	      || att == "function"
	      || att == "party"
	      || att == "role"
	      || att == "party-ref"
	      || att == "member-ref" ){
      KWargs args;
      args["subset"] = att;
      args["class"] = cls;
      div->add_child<Feature>( args );
    }
    else {
#pragma omp critical
      {
	cerr << "unsupported attribute: " << att << " on speech: "
	     << id << endl;
      }
    }
  }

  xmlNode *p = speech->children;
  while ( p ){
    string label = TiCC::Name(p);
    if ( label == "p" ){
      list<Note*> notes;
      Paragraph *par = add_par( div, p, notes );
      if ( !notes.empty() ){
	add_notes( par, notes );
      }
    }
    else if ( label == "stage-direction" ){
      process_stage( div, p );
    }
    else if ( label == "speech" ){
      process_speech( div, p );
    }
    else {
#pragma omp critical
      {
	cerr << "speech: " << id << ", unhandled: " << label << endl;
	cerr << "speech-content " << TiCC::TextValue(p) << endl;
      }
    }
    p = p->next;
  }
}


void add_actor( FoliaElement *root, const xmlNode *act ){
  KWargs d_args;
  d_args["class"] = "actor";
  d_args["processor"] = processor_id;
  Division *div = root->add_child<Division>( d_args );
  xmlNode *p = act->children;
  while ( p ){
    string label = TiCC::Name(p);
    if ( label == "person" ){
      string speaker = TiCC::getAttribute( p, "speaker" );
      if ( speaker.empty() ){
	speaker = "unknown";
      }
      string ref = TiCC::getAttribute( p, "member-ref" );
      if ( ref.empty() ){
	ref = "unknown";
      }
      KWargs args;
      args["subset"] = "speaker";
      args["class"] = speaker;
      div->add_child<Feature>( args );
      args["subset"] = "member-ref";
      args["class"] = ref;
      div->add_child<Feature>( args );
    }
    else if ( label == "organization" ){
      string name = TiCC::getAttribute( p, "name" );
      if ( name.empty() ){
	name = "unknown";
      }
      string function = TiCC::getAttribute( p, "function" );
      if ( function.empty() ){
	function = "unknown";
      }
      string ref = TiCC::getAttribute( p, "ref" );
      KWargs args;
      args["subset"] = "name";
      args["class"] = name;
      div->add_child<Feature>( args );
      args["subset"] = "function";
      args["class"] = function;
      div->add_child<Feature>( args );
      if ( !ref.empty() ){
	args["subset"] = "ref";
	args["class"] = ref;
	div->add_child<Feature>( args );
      }
    }
    else {
#pragma omp critical
      {
	cerr << "add actor, unhandled :" << label << endl;
      }
    }
    p = p->next;
  }
}


void add_submit( FoliaElement *root, const xmlNode *sm ){
  KWargs args;
  args["class"] = "submitted-by";
  args["processor"] = processor_id;
  Division *div = root->add_child<Division>( args );
  xmlNode *p = sm->children;
  while ( p ){
    string label = TiCC::Name(p);
    if ( label == "actor" ){
      add_actor( div, p );
    }
    else {
#pragma omp critical
      {
	cerr << "add sumitted, unhandled :" << label << endl;
      }
    }
    p = p->next;
  }
}

void add_information( FoliaElement *root, const xmlNode *info ){
  KWargs d_args;
  d_args["class"] = "information";
  d_args["processor"] = processor_id;
  Division *div = root->add_child<Division>( d_args );
  xmlNode *p = info->children;
  while ( p ){
    string label = TiCC::Name(p);
    if ( label == "dossiernummer"
	 || label == "ondernummer"
	 || label == "rijkswetnummer"
	 || label == "introduction"
	 || label == "part"
	 || label == "outcome" ){
      string cls = TiCC::TextValue( p );
      if ( cls.empty() ){
	cls = "unknown";
      }
      KWargs args;
      args["subset"] = label;
      args["class"] = cls;
      div->add_child<Feature>( args );
    }
    else if ( label == "submitted-by" ){
      add_submit( div, p );
    }
    else {
#pragma omp critical
      {
	cerr << "add information, unhandled :" << label << endl;
      }
    }
    p = p->next;
  }
}

void add_about( Division *root, xmlNode *p ){
  KWargs atts = getAllAttributes( p );
  if ( verbose ){
#pragma omp critical
    {
      using TiCC::operator<<;
      cerr << "add_vote, atts=" << atts << endl;
    }
  }
  string title = atts["title"];
  string voted_on = atts["voted_on"];
  string ref =  atts["ref"];
  KWargs d_args;
  d_args["class"] = "about";
  d_args["processor"] = processor_id;
  Division *div = root->add_child<Division>( d_args );
  if ( !title.empty() ){
    KWargs args;
    args["class"] = title;
    args["subset"] = "title";
    div->add_child<Feature>( args );
  }
  if ( !voted_on.empty() ){
    KWargs args;
    args["class"] = voted_on;
    args["subset"] = "voted-on";
    div->add_child<Feature>( args );
  }
  if ( !ref.empty() ){
    KWargs args;
    args["class"] = ref;
    args["subset"] = "ref";
    div->add_child<Feature>( args );
  }
  p = p->children;
  while ( p ){
    string tag = TiCC::Name( p );
    if ( tag == "information" ){
      add_information( div, p );
    }
    else {
#pragma omp critical
      {
	cerr << "about vote, unhandled: " << tag << endl;
      }
    }
    p = p->next;
  }
}

void process_vote( Division *div, xmlNode *vote ){
  KWargs atts = getAllAttributes( vote );
  string vote_type = atts["vote-type"];
  if ( vote_type.empty() ){
    vote_type = "unknown";
  }
  string outcome = atts["outcome"];
  if ( outcome.empty() ){
    outcome = "unknown";
  }
  string id = atts["id"];
  if ( verbose ){
#pragma omp critical
    {
      cerr << "process_vote: id = " << id << endl;
    }
  }
  KWargs vt_args;
  vt_args["class"] = vote_type;
  vt_args["subset"] = "vote-type";
  div->add_child<Feature>( vt_args );
  vt_args["class"] = outcome;
  vt_args["subset"] = "outcome";
  div->add_child<Feature>( vt_args );
  xmlNode *p = vote->children;
  while ( p ){
    string label = TiCC::Name(p);
    if ( label == "about" ){
      add_about( div, p );
    }
    else if ( label == "consequence" ){
      string cls = TiCC::TextValue( p );
      if ( cls.empty() ){
	cls = "unknown";
      }
      KWargs args;
      args["subset"] = label;
      args["class"] = cls;
      div->add_child<Feature>( args );
    }
    else if ( label == "division" ){
#pragma omp critical
      {
	cerr << "vote: skip division stuff" << endl;
      }
    }
    else {
#pragma omp critical
      {
	cerr << "vote: " << id << ", unhandled: " << label << endl;
      }
    }
    p = p->next;
  }
}

void process_scene( Division *root, xmlNode *scene ){
  KWargs atts = getAllAttributes( scene );
  string id = atts["id"];
  if ( verbose ){
#pragma omp critical
    {
      cerr << "process_scene: id=" << id << endl;
    }
  }
  string type = atts["type"];
  KWargs scene_args;
  scene_args["xml:id"] = id;
  scene_args["class"] = type;
  scene_args["processor"] = processor_id;
  Division *div = root->add_child<Division>( scene_args );
  for ( const auto& [att,cls] : atts ){
    if ( att == "id"
	 || att == "type" ){
      continue;
    }
    else if ( att == "speaker"
	      || att == "function"
	      || att == "party"
	      || att == "role"
	      || att == "party-ref"
	      || att == "member-ref" ){
      KWargs args;
      args["subset"] = att;
      args["class"] = cls;
      div->add_child<Feature>( args );
    }
    else {
#pragma omp critical
      {
	cerr << "unsupported attribute: " << att << " on scene:"
	     << id << endl;
      }
    }
  }

  xmlNode *p = scene->children;
  while ( p ){
    string label = TiCC::Name(p);
    if ( label == "speech" ){
      process_speech( div, p );
    }
    else if ( label == "stage-direction" ){
      process_stage( div, p );
    }
    else {
#pragma omp critical
      {
	cerr << "scene: " << id << ", unhandled: " << label << endl;
      }
    }
    p = p->next;
  }
}

void process_break( Division *root, xmlNode *brk ){
  if ( verbose ){
#pragma omp critical
    {
      cerr << "process_break" << endl;
    }
  }
  KWargs args;
  args["processor"] = processor_id;
  root->doc()->declare( folia::AnnotationType::LINEBREAK, setname, args );
  args.clear();
  args["pagenr"] = TiCC::getAttribute( brk, "originalpagenr");
  args["newpage"] = "yes";
  Linebreak *pb = root->add_child<Linebreak>( args );
  args.clear();
  args["class"] = "page";
  args["format"] = "image/jpeg";
  args["xlink:href"] = TiCC::getAttribute( brk, "source");
  pb->add_child<Relation>( args );
}

void process_members( Division *root, xmlNode *members ){
  ForeignData *fd = root->add_child<ForeignData>();
  fd->set_data( members );
}

void add_h_c_t( FoliaElement *root, xmlNode *block ){
  string id = TiCC::getAttribute( block, "id" );
  string type = TiCC::getAttribute( block, "type" );
  KWargs args;
  args["xml:id"] = id;
  args["class"] = type;
  args["processor"] = processor_id;
  Division *div = root->add_child<Division>( args );
  xmlNode *p = block->children;
  while ( p ){
    string label = TiCC::Name(p);
    if ( label == "p" ){
      list<Note*> notes;
      Paragraph *par = add_par( div, p, notes );
      if ( !notes.empty() ){
	add_notes( par, notes );
      }
    }
    else if ( label == "stage-direction" ){
      process_stage( div, p );
    }
    else {
#pragma omp critical
      {
	cerr << "create_" << type << ", unhandled :" << label
	     << " id=" << id << endl;
      }
    }
    p = p->next;
  }
}

void process_stage( Division *root, xmlNode *_stage ){
  KWargs args;
  string stage_id = TiCC::getAttribute( _stage, "id" );
  string stage_type = TiCC::getAttribute( _stage, "type" );
  if ( verbose ){
#pragma omp critical
    {
      cerr << "process_stage: " << stage_type << " ID=" << stage_id << endl;
    }
  }
  args["xml:id"] = stage_id;
  if ( stage_type.empty() ){
    args["class"] = "stage-direction";
  }
  else {
    args["class"] = stage_type;
  }
  args["processor"] = processor_id;
  Division *div = root->add_child<Division>( args );
  xmlNode *stage = _stage->children;
  while ( stage ){
    string id = TiCC::getAttribute( stage, "id" );
    string type = TiCC::getAttribute( stage, "type" );
    string label = TiCC::Name( stage );
    if ( verbose ){
#pragma omp critical
      {
	cerr << "process_stage: LOOP:" << label << " type=" << type << " ID=" << id << endl;
      }
    }
    if ( type == "chair" || label == "chair" ){
      process_chair( div, stage );
    }
    else if ( type == "pagebreak" ){
      process_break( div, stage->children );
    }
    else if ( type == "header"
	      || type == "title"
	      || type == "subtitle"
	      || type == "other"
	      || type == "question"
	      || type == "table"
	      || type == "label"
	      || type == "motie"
	      || type == "footer"
	      || type == "subject" ){
      add_h_c_t( div, stage );
    }
    else if ( type == "speech" ){
      process_speech( div, stage );
    }
    else if ( label == "vote" ){
      process_vote( div, stage );
    }
    else if ( label == "stage-direction" ){
      process_stage( div, stage );
    }
    else if ( type == "" ){ //nested or?
      if ( label == "stage-direction" ){
	process_stage( div, stage );
      }
      else if ( label == "p" ){
	list<Note*> notes;
	Paragraph *par = add_par( div, stage, notes );
	if ( !notes.empty() ){
	  add_notes( par, notes );
	}
      }
      else if ( label == "pagebreak" ){
	process_break( div, stage );
      }
      else if ( label == "speech" ){
	process_speech( div, stage );
      }
      else if ( label == "members" ){
	process_members( div, stage );
      }
      else {
#pragma omp critical
	{
	  cerr << "stage-direction: " << id << ", unhandled nested: "
	       << label << endl;
	}
      }
    }
    else {
#pragma omp critical
      {
	cerr << "stage-direction: " << id << ", unhandled type: "
	     << type << endl;
      }
    }
    stage = stage->next;
  }
}

folia::Document *create_basedoc( const string& docid,
				 const string& command,
				 xmlNode *metadata = 0,
				 xmlNode *docinfo = 0 ){
  Document *doc = new Document( "xml:id='" + docid + "'" );
  processor *proc = add_provenance( *doc, "FoLiA-pm", command );
  KWargs args;
  if ( processor_id.empty() ){
    processor_id = proc->id();
  }
  args["processor"] = processor_id;
  doc->declare( folia::AnnotationType::DIVISION, setname, args );
  doc->declare( folia::AnnotationType::RELATION, setname, args );
  if ( metadata ){
    if ( metadata->nsDef == 0 ){
      xmlNewNs( metadata,
		folia::to_xmlChar("http://www.politicalmashup.nl"),
		0 );
    }
    doc->set_foreign_metadata( metadata );
  }
  if ( docinfo ){
    doc->set_foreign_metadata( docinfo );
  }
  return doc;
}

void process_topic( const string& outDir,
		    const string& prefix,
		    const string& command,
		    Text* base_text,
		    xmlNode *topic,
		    bool no_split ){
  string id = TiCC::getAttribute( topic, "id" );
  if ( verbose ){
#pragma omp critical
    {
      cerr << "process_topic: id=" << id << endl;
    }
  }
  KWargs args;
  Document *doc = 0;
  FoliaElement *root;
  if ( no_split ){
    doc = base_text->doc();
    args["generate_id"] = base_text->id();
    args["class"] = "proceedings";
    args["processor"] = processor_id;
    root = base_text->add_child<Division>( args );
  }
  else {
    id = prefix + id;
    doc = create_basedoc( id, command );
    args["xml:id"] = id + ".text";
    root = doc->create_root<Text>( args );
  }
  args.clear();
  args["xml:id"] = id + ".div";
  args["class"] = "topic";
  args["processor"] = processor_id;
  Division *div = root->add_child<Division>( args );
  string title = TiCC::getAttribute( topic, "title" );
  if ( !title.empty() ){
    args.clear();
    args["processor"] = processor_id;
    doc->declare( folia::AnnotationType::HEAD,
		  setname, args );
    args.clear();
    Head *hd = div->add_child<Head>( );
    hd->settext( title );
  }
  xmlNode *p = topic->children;
  while ( p ){
    string tag = TiCC::Name(p);
    if ( tag == "stage-direction" ){
      process_stage( div, p );
    }
    else if ( tag == "speech" ){
      process_speech( div, p );
    }
    else if ( tag == "scene" ){
      process_scene( div, p );
    }
    else {
#pragma omp critical
      {
	cerr << "topic: " << id << ", unhandled: " << tag << endl;
      }
    }
    p = p->next;
  }
  if ( !no_split ){
    string filename = outDir+id+".folia.xml";
    doc->save( filename );
    delete doc;
#pragma omp critical
    {
      cout << "saved external file: " << filename << endl;
    }

    args.clear();
    args["xml:id"] = id;
    args["src"] = id + ".folia.xml";
    External *ext = new External( args );
    base_text->append( ext );
  }
}

void process_proceeding( const string& outDir,
			 const string& prefix,
			 const string& command,
			 Text *root,
			 xmlNode *proceed,
			 bool no_split ){
  string id = TiCC::getAttribute( proceed, "id" );
  if ( verbose ){
#pragma omp critical
    {
      cerr << "process_proceeding: id=" << id << endl;
    }
  }
  list<xmlNode*> topics = TiCC::FindNodes( proceed, "*:topic" );
  for ( const auto& topic : topics ){
    process_topic( outDir, prefix, command, root, topic, no_split );
  }
}

void process_sub_block( Division *, const xmlNode * );

void add_signed( FoliaElement *root, xmlNode* block ){
  string id = TiCC::getAttribute( block, "id" );
  string type = TiCC::getAttribute( block, "type" );
  Document *doc = root->doc();
  KWargs args;
  args["processor"] = processor_id;
  doc->declare( folia::AnnotationType::ENTITY,
		setname, args );
  args.clear();
  args["xml:id"] = id;
  args["class"] = type;
  args["processor"] = processor_id;
  Division *div = root->add_child<Division>( args );
  EntitiesLayer *el = div->add_child<EntitiesLayer>();
  xmlNode *p = block->children;
  while ( p ){
    string label = TiCC::Name(p);
    if ( label == "p" ){
      add_entity( el, p );
    }
    else {
#pragma omp critical
      {
	cerr << "create_signed: " << id << ", unhandled :" << label
	     << " type=" << type << endl;
      }
    }
    p = p->next;
  }
}

void add_section( FoliaElement *root, xmlNode* block ){
  string id = TiCC::getAttribute( block, "id" );
  string type = TiCC::getAttribute( block, "type" );
  string section_id = TiCC::getAttribute( block, "section-identifier" );
  string section_path =TiCC::getAttribute( block, "section-path" );
  KWargs args;
  args["xml:id"] = id;
  args["class"] = type;
  args["processor"] = processor_id;
  if ( !section_id.empty() ){
    args["n"] = section_id;
  }
  Division *div = root->add_child<Division>( args );
  if ( !section_path.empty() ){
    args.clear();
    args["subset"] = "section-path";
    args["class"] = section_path;
    div->add_child<Feature>( args );
  }
  process_sub_block( div, block );
}

void add_block( FoliaElement *root, xmlNode *block ){
  string id = TiCC::getAttribute( block, "id" );
  string type = TiCC::getAttribute( block, "type" );
  KWargs args;
  args["xml:id"] = id;
  args["class"] = type;
  args["processor"] = processor_id;
  Division *div = root->add_child<Division>( args );
  process_sub_block( div, block );
}

void add_heading( FoliaElement *root, xmlNode *block ){
  FoliaElement *el = new Head( );
  try {
    root->append( el );
  }
  catch (...){
    // so if another Head is already there, append it as a Div
    KWargs args;
    args["class"] = "subheading";
    args["processor"] = processor_id;
    root->add_child<Division>( args );
  }
  string txt = TiCC::TextValue(block);
  if ( !txt.empty() ){
    el->settext( txt );
  }
}

void process_sub_block( Division *root, const xmlNode *_block ){
  KWargs args;
  xmlNode *block = _block->children;
  list<Note*> notes;
  while ( block ){
    string id = TiCC::getAttribute( block, "id" );
    string type = TiCC::getAttribute( block, "type" );
    string label = TiCC::Name(block);
    if ( verbose ){
#pragma omp critical
      {
	cerr << "process_block_children: id=" << id
	     << " tag=" << label << " type=" << type << endl;
      }
    }
    if ( type == "signed-by" ){
      add_signed( root, block );
    }
    else if ( label == "p" ){
      add_par( root, block, notes );
    }
    else if ( label == "heading" ){
      add_heading( root, block );
    }
    else if ( type == "header"
	      || type == "content"
	      || type == "title" ){
      add_h_c_t( root, block );
    }
    else if ( type == "section" ){
      add_section( root, block );
    }
    else if ( label == "block" ){
      add_block( root, block );
    }
    else {
#pragma omp critical
      {
	cerr << "block: " << id << ", unhandled type: "
	     << type << endl;
      }
    }
    block = block->next;
  }
  if ( !notes.empty() ){
    add_notes( root, notes );
  }
}

void process_block( Text* base_text,
		    xmlNode *block ){
  string id = TiCC::getAttribute( block, "id" );
  string type = TiCC::getAttribute( block, "type" );
  if ( verbose ){
#pragma omp critical
    {
      cerr << "process_block: id=" << id << " type=" << type << endl;
    }
  }
  KWargs args;
  args["xml:id"] = id;
  args["class"] = type;
  args["processor"] = processor_id;
  Division *div = base_text->add_child<Division>( args );
  process_sub_block( div, block );
}

void process_parldoc( Text *root,
		      xmlNode *pdoc ){
  string id = TiCC::getAttribute( pdoc, "id" );
  if ( verbose ){
#pragma omp critical
    {
      cerr << "process_parldoc: id=" << id << endl;
    }
  }
  list<xmlNode*> blocks = TiCC::FindNodes( pdoc, "*:block" );
  for ( const auto& block : blocks ){
    process_block( root, block );
  }
}

void convert_to_folia( const string& file,
		       const string& outDir,
		       const string& prefix,
		       const string& command,
		       bool no_split ){
  bool succes = true;
#pragma omp critical
  {
    cout << "converting: " << file << endl;
  }
  xmlDoc *xmldoc = xmlReadFile( file.c_str(),
				0,
				::XML_PARSER_OPTIONS );
  if ( xmldoc ){
    xmlNode *root = xmlDocGetRootElement( xmldoc );
    xmlNode *metadata = TiCC::xPath( root, "//meta" );
    xmlNode *docinfo = TiCC::xPath( root, "//*[local-name()='docinfo']" );
    if ( !metadata ){
#pragma omp critical
      {
	cerr << "no metadata" << endl;
      }
      succes = false;
    }
    else {
      string base = TiCC::basename( file );
      string docid = prefix + base;
      Document *doc = create_basedoc( docid, command, metadata, docinfo );
      string::size_type pos = docid.rfind( ".xml" );
      if ( pos != string::npos ){
	docid.resize(pos);
      }
      KWargs args;
      args["xml:id"] = docid + ".text";
      Text *text = doc->create_root<Text>( args );
      try {
	xmlNode *p = root->children;
	while ( p ){
	  if ( TiCC::Name( p ) == "proceedings" ){
	    process_proceeding( outDir, prefix, command, text, p, no_split );
	  }
	  else if ( TiCC::Name( p ) == "parliamentary-document" ){
	    process_parldoc( text, p );
	  }
	  p = p->next;
	}
	string outname = outDir + docid + ".folia.xml";
#pragma omp critical
	{
	  cout << "save " << outname << endl;
	}
	doc->save( outname );
      }
      catch ( const exception& e ){
#pragma omp critical
	{
	  cerr << "error processing " << file << endl
	       << e.what() << endl;
	  succes = false;
	}
      }
      delete doc;
    }
    xmlFreeDoc( xmldoc );
  }
  else {
#pragma omp critical
    {
      cerr << "XML failed: " << file << endl;
    }
    succes = false;
  }
  if ( !succes ){
#pragma omp critical
    {
      cerr << "FAILED: " << file << endl;
    }
  }
}


void usage(){
  cerr << "Usage: FoLiA-pm [options] files/dir" << endl;
  cerr << "\t convert Political Mashup XML files to FoLiA" << endl;
  cerr << "\t when a dir is given, all '.xml' files in that dir are processed"
       << endl;
  cerr << "\t-t or --threads <threads>\t Number of threads to run on." << endl;
  cerr << "\t\t\t If 'threads' has the value \"max\", the number of threads is set to a" << endl;
  cerr << "\t\t\t reasonable value. (OMP_NUM_TREADS - 2)" << endl;
  cerr << "\t--nosplit\t don't create separate topic files" << endl;
  cerr << "\t--prefix='pre'\t add this prefix to ALL created files. (default 'FPM-') " << endl;
  cerr << "\t\t\t use 'none' for an empty prefix. (can be dangerous)" << endl;
  cerr << "\t-O\t\t output directory " << endl;
  cerr << "\t-v\t\t verbose output " << endl;
  cerr << "\t-V or --version\t show version " << endl;
  cerr << "\t-h or --help \t this messages " << endl;
}

int main( int argc, char *argv[] ){
  TiCC::CL_Options opts;
  try {
    opts.set_short_options( "vVt:O:h" );
    opts.set_long_options( "nosplit,help,version,prefix:,threads:" );
    opts.init( argc, argv );
  }
  catch( TiCC::OptionError& e ){
    cerr << e.what() << endl;
    usage();
    exit( EXIT_FAILURE );
  }
  string outputDir;
  if ( opts.extract( 'h' )
       || opts.extract( "help" ) ){
    usage();
    exit(EXIT_SUCCESS);
  }
  if ( opts.extract('V' )
       || opts.extract( "version" ) ){
    cerr << PACKAGE_STRING << endl;
    exit(EXIT_SUCCESS);
  }
  const string command = "FoLiA-pm " + opts.toString();
  verbose = opts.extract( 'v' );
  string value;
  if ( opts.extract( 't', value )
       || opts.extract( "threads", value ) ){
#ifdef HAVE_OPENMP
    int numThreads=1;
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
  opts.extract( 'O', outputDir );
  if ( !outputDir.empty() && outputDir[outputDir.length()-1] != '/' )
    outputDir += "/";
  bool no_split = opts.extract( "nosplit" );
  if ( !opts.empty() ){
    cerr << "unsupported options : " << opts.toString() << endl;
    usage();
    exit(EXIT_FAILURE);
  }
  string prefix = "FPM-";
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
  if ( !outputDir.empty() ){
    if ( !TiCC::isDir(outputDir) ){
      if ( !TiCC::createPath( outputDir ) ){
	cerr << "outputdir '" << outputDir
	     << "' doesn't exist and can't be created" << endl;
	exit(EXIT_FAILURE);
      }
    }
  }
  if ( fileNames.size() == 1 ){
    string name = fileNames[0];
    if ( !( TiCC::isFile(name) || TiCC::isDir(name) ) ){
      cerr << "'" << name << "' doesn't seem to be a file or directory"
	   << endl;
      exit(EXIT_FAILURE);
    }
    if ( !TiCC::isFile(name) ){
      fileNames = TiCC::searchFilesMatch( name, "*.xml" );
    }
  }
  else {
    // sanity check
    auto it = fileNames.begin();
    while ( it != fileNames.end() ){
      if ( it->find( ".xml" ) == string::npos ){
	if ( verbose ){
	  cerr << "skipping file: " << *it << endl;
	}
	it = fileNames.erase(it);
      }
      else {
	++it;
      }
    }
  }
  size_t toDo = fileNames.size();
  cout << "start processing of " << toDo << " files " << endl;

#pragma omp parallel for shared(fileNames) schedule(dynamic)
  for ( size_t fn=0; fn < fileNames.size(); ++fn ){
    convert_to_folia( fileNames[fn], outputDir, prefix, command, no_split );
  }
  cout << "done" << endl;
  exit(EXIT_SUCCESS);
}
