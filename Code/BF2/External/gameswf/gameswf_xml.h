// gameswf_xml.h -- Rob Savoye <rob@welcomehome.org> 2005
// This source code has been donated to the Public Domain.

#ifndef GAMESWF_XML_H
#define GAMESWF_XML_H

#include "gameswf_action.h"

namespace gameswf
{
    // [INFERRED] Native names and placement for DICE's XML builtins.
    // [CHOSEN, not recovered] S3 bridge: typed native objects replace the
    // retail embedded String/attribute objects and garbage-collected raw child links.
    // These declarations do not reproduce the retail x86 object layout.
    struct xml_node : public as_object
    {
        xml_node(const char* name = NULL, int type = 0, xml_node* parent = NULL);
        // SwiffPlayer.dll 0x100A8E20 destroys attributes, name, then value before
        // the object base; deleting wrappers 0x100A94F0 (xml_node) and 0x100A9530
        // (xml_document). The reverse declaration order below gives the same order;
        // the native child/sibling links additionally release their nodes.
        virtual bool get_member(const tu_stringi& name, as_value* value);
        virtual void enumerate(as_environment* env);
        virtual void clear_children();
        virtual void insert_sibling(xml_node* node);

        int m_node_type;
        smart_ptr<as_object_interface> m_node_value;
        smart_ptr<as_object_interface> m_node_name;
        smart_ptr<as_object> m_attributes;
        xml_node* m_parent;
        xml_node* m_previous_sibling;
        smart_ptr<xml_node> m_next_sibling;
        smart_ptr<xml_node> m_first_child;
    };

    struct xml_document : public xml_node
    {
        xml_document();
        virtual void set_member(const tu_stringi& name, const as_value& value);
        virtual bool get_member(const tu_stringi& name, as_value* value);
        virtual void clear_children();

        // Parser callbacks keep one node per character-data callback.
        void start_element(const char* name, const char** attributes);
        void end_element(const char* name);
        void character_data(const char* text, int length);

        // [CHOSEN, not recovered] S3 bridge exposing the load path's parser
        // sequence to native callers. Does not clear children or finalize input.
        // Returns true exactly when the parser returns 1; updates status.
        bool parse(const char* text, int length);

        xml_node* m_current;
        bool m_ignore_white;
        int m_status;
    };

    void xml_new(const fn_call& fn);
    void xmlnode_new(const fn_call& fn);
    void xml_load(const fn_call& fn);
    void xml_ondata(const fn_call& fn);

    // [CHOSEN, not recovered] issue #7 / S3 bridge. Call while the existing
    // release_script_objects registry snapshot holds every live object.
    // Required before publishing the factories in the action layer.
    void release_xml_script_references(as_object* object);
}

#endif
