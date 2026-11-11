// gameswf_xml.h -- Rob Savoye <rob@welcomehome.org> 2005
// This source code has been donated to the Public Domain. Do whatever you want with it.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "gameswf_xml.h"
#include "gameswf_log.h"
#include "base/tu_file.h"
#include "expat/expat.h"

namespace gameswf
{
    namespace
    {
        // [INFERRED] Name/placement of the small C++ parser facade used by XML.
        // [CHOSEN, not recovered] The native facade has only the used methods;
        // it does not reproduce the retail parser's 30-slot vtable or x86 layout.
        class dice_xml_parser
        {
        public:
            static dice_xml_parser* create(const char* encoding)
            {
                // SwiffPlayer.dll 0x100AF2B0: allocate, construct, init; release
                // on init failure. Allocation size follows the native adapter.
                void* storage = std::malloc(sizeof(dice_xml_parser));
                if (!storage) return NULL;
                dice_xml_parser* parser = new (storage) dice_xml_parser;
                if (!parser->init(encoding)) { parser->release(); return NULL; }
                return parser;
            }

            void set_element_handler(xml_document* handler)
            {
                // SwiffPlayer.dll 0x100AA7E0 stores the element receiver only.
                m_element_handler = handler;
            }

            void set_character_data_handler(xml_document* handler)
            {
                // SwiffPlayer.dll 0x100AA7F0 stores the character-data receiver.
                m_character_data_handler = handler;
            }

            int parse(const char* text, int length, int final)
            {
                // [CHOSEN, not recovered] C-API adapter, not the retail parser body.
                return XML_Parse(m_parser, text, length, final);
            }

            void release()
            {
                // SwiffPlayer.dll 0x100AD520 destroys then frees the allocation.
                // [CHOSEN, not recovered] C-API cleanup replaces internal pools.
                XML_ParserFree(m_parser);
                this->~dice_xml_parser();
                std::free(this);
            }

        private:
            dice_xml_parser()
                : m_parser(NULL), m_element_handler(NULL), m_character_data_handler(NULL)
            {
                // SwiffPlayer.dll 0x100AC230 initializes parser-owned storage.
                // [CHOSEN, not recovered] That storage belongs to the C parser;
                // retain only its handle and the two observed handler receivers.
            }

            bool init(const char* encoding)
            {
                // SwiffPlayer.dll 0x100AF070 clears handlers/state, initializes
                // encoding and storage, and reports allocation failure.
                // [CHOSEN, not recovered] XML_ParserCreate owns those internals
                // in the approved substitute; no retail pools are reconstructed.
                m_parser = XML_ParserCreate(encoding);
                if (!m_parser) return false;
                XML_SetUserData(m_parser, this);
                XML_SetElementHandler(m_parser, start_element, end_element);
                XML_SetCharacterDataHandler(m_parser, character_data);
                return true;
            }

            // [CHOSEN, not recovered] C callbacks adapt retail's C++ handler
            // receivers to the existing native XML document methods.
            static void start_element(void* receiver, const char* name, const char** attributes)
            {
                dice_xml_parser* parser = static_cast<dice_xml_parser*>(receiver);
                if (parser->m_element_handler)
                    parser->m_element_handler->start_element(name, attributes);
            }
            static void end_element(void* receiver, const char* name)
            {
                dice_xml_parser* parser = static_cast<dice_xml_parser*>(receiver);
                if (parser->m_element_handler) parser->m_element_handler->end_element(name);
            }
            static void character_data(void* receiver, const char* text, int length)
            {
                dice_xml_parser* parser = static_cast<dice_xml_parser*>(receiver);
                if (parser->m_character_data_handler)
                    parser->m_character_data_handler->character_data(text, length);
            }

            XML_Parser m_parser;
            xml_document* m_element_handler;
            xml_document* m_character_data_handler;
        };
    }

    // [CHOSEN, not recovered] S3 bridge to the existing VM dispatcher, whose
    // one-argument convenience wrapper is declared but not defined in this tree.
    as_value call_method(const as_value& method, as_environment* env,
                         as_object_interface* receiver, int nargs, int first);

    static void xml_set_int(as_value* result, int number)
    {
        // [CHOSEN, not recovered] S3 bridge: native values own one reference;
        // they lack retail's ownership-discriminator byte. set_int preserves
        // two retail release calls, so retain one extra native reference first.
        if (result->get_type() == as_value::OBJECT && result->m_object_value)
            result->m_object_value->add_ref();
        else if (result->get_type() == as_value::AS_FUNCTION && result->m_as_function_value)
            result->m_as_function_value->add_ref();
        result->set_int(number);
    }

    static void xml_call_method1(const as_value& method, as_environment* env,
                                 as_object_interface* receiver, const as_value& argument)
    {
        // [CHOSEN, not recovered] S3 bridge: use native stack/dispatch operations.
        // The retail 0x1008EAD0 wrapper leaves its appended argument on the stack.
        env->push(argument);
        call_method(method, env, receiver, 1, env->get_top_index());
    }

    static void xml_has_child_nodes(const fn_call& fn)
    {
        // SwiffPlayer.dll 0x100A83A0 reads the child link before replacing result.
        const bool present = static_cast<xml_node*>(fn.this_ptr)->m_first_child.get_ptr() != NULL;
        fn.result->set_bool(present);
    }

    static void xml_append_child(const fn_call& fn)
    {
        // SwiffPlayer.dll 0x100A8470: no arity/type check, detach or result assignment.
        xml_node* self = static_cast<xml_node*>(fn.this_ptr);
        xml_node* child = static_cast<xml_node*>(fn.env->bottom(fn.first_arg_bottom_index).to_object());
        child->m_parent = self;
        xml_node* tail = self->m_first_child.get_ptr();
        if (tail)
        {
            while (tail->m_next_sibling.get_ptr()) tail = tail->m_next_sibling.get_ptr();
            tail->insert_sibling(child);
        }
        else self->m_first_child = child;
    }

    static void xml_create_element(const fn_call& fn)
    {
        // SwiffPlayer.dll 0x100A84C0 allocates before converting the first argument.
        // [CHOSEN, not recovered] S3 bridge to native allocation/construction.
        void* storage = ::operator new(sizeof(xml_node));
        const char* name = fn.env->bottom(fn.first_arg_bottom_index).to_string();
        xml_node* node = new (storage) xml_node(name, 1, NULL);
        fn.result->set_as_object_interface(node);
    }

    xml_node::xml_node(const char* name, int type, xml_node* parent)
        : m_node_value(create_dice_string_object(NULL, 0)),
          m_node_name(create_dice_string_object(NULL, 0)),
          m_attributes(new as_object),
          m_parent(NULL), m_previous_sibling(NULL)
    {
        // SwiffPlayer.dll 0x100A8EB0: value, name, attributes; raw links cleared;
        // hasChildNodes, appendChild, createElement installed before linking parent.
        m_node_type = type;
        if (name) assign_dice_string_object_narrow(m_node_name.get_ptr(), name);
        as_object::set_member("hasChildNodes", &xml_has_child_nodes);
        as_object::set_member("appendChild", &xml_append_child);
        as_object::set_member("createElement", &xml_create_element);
        if (parent)
        {
            m_parent = parent;
            xml_node* tail = parent->m_first_child.get_ptr();
            if (tail)
            {
                while (tail->m_next_sibling.get_ptr()) tail = tail->m_next_sibling.get_ptr();
                tail->m_next_sibling = this;
                m_previous_sibling = tail;
            }
            else parent->m_first_child = this;
        }
    }

    void xml_node::clear_children()
    {
        // SwiffPlayer.dll 0x100A8350 clears only the head link.
        m_first_child = NULL;
    }

    void xml_node::insert_sibling(xml_node* node)
    {
        // SwiffPlayer.dll 0x100A8360 / XML override 0x100A8420.
        if (m_next_sibling.get_ptr())
        {
            m_next_sibling->m_previous_sibling = node;
            node->m_next_sibling = m_next_sibling;
        }
        m_next_sibling = node;
        node->m_previous_sibling = this;
    }

    bool xml_node::get_member(const tu_stringi& name, as_value* value)
    {
        // SwiffPlayer.dll 0x100A8920. Builtin names are case sensitive;
        // absent links and nodeType zero fall through to the ordinary object bag.
        const char* text = name.c_str();
        if (*text)
        {
            const char* end = text;
            while (*end >= '0' && *end <= '9') ++end;
            if (!*end)
            {
                int index = std::atoi(text);
                xml_node* child = m_first_child.get_ptr();
                while (index && child) { --index; child = child->m_next_sibling.get_ptr(); }
                if (child) value->set_as_object_interface(child);
                else value->set_undefined();
                return true;
            }
        }
        if (std::strcmp(text, "childNodes") == 0)
        {
            // [CHOSEN, not recovered] S3 bridge to the action layer's private Array type.
            smart_ptr<as_object> children = create_array_object();
            for (xml_node* child = m_first_child.get_ptr(); child; child = child->m_next_sibling.get_ptr())
                array_object_push(children.get_ptr(), as_value(child));
            value->set_as_object_interface(children.get_ptr());
            return true;
        }
        if (std::strcmp(text, "attributes") == 0) value->set_as_object_interface(m_attributes.get_ptr());
        else if (std::strcmp(text, "nodeValue") == 0) value->set_as_object_interface(m_node_value.get_ptr());
        else if (std::strcmp(text, "nodeName") == 0) value->set_as_object_interface(m_node_name.get_ptr());
        else if (std::strcmp(text, "nodeType") == 0 && m_node_type) xml_set_int(value, m_node_type);
        else if (std::strcmp(text, "parentNode") == 0 && m_parent) value->set_as_object_interface(m_parent);
        else if (std::strcmp(text, "lastChild") == 0 && m_first_child.get_ptr())
        {
            xml_node* child = m_first_child.get_ptr();
            while (child->m_next_sibling.get_ptr()) child = child->m_next_sibling.get_ptr();
            value->set_as_object_interface(child);
        }
        else if (std::strcmp(text, "firstChild") == 0 && m_first_child.get_ptr())
            value->set_as_object_interface(m_first_child.get_ptr());
        else if (std::strcmp(text, "nextSibiling") == 0 && m_next_sibling.get_ptr())
            value->set_as_object_interface(m_next_sibling.get_ptr());
        else if (std::strcmp(text, "previousSibiling") == 0 && m_previous_sibling)
            value->set_as_object_interface(m_previous_sibling);
        else return as_object::get_member(name, value);
        return true;
    }

    void xml_node::enumerate(as_environment* env)
    {
        // SwiffPlayer.dll 0x100A8D50 enumerates only child indices as strings.
        int index = 0;
        for (xml_node* child = m_first_child.get_ptr(); child; child = child->m_next_sibling.get_ptr())
        {
            char name[32];
            std::sprintf(name, "%d", index++);
            env->push(as_value(name));
        }
    }

    xml_document::xml_document() : m_current(this), m_ignore_white(false), m_status(-1)
    {
        // SwiffPlayer.dll 0x100A9160 builds an unnamed type-zero XMLNode;
        // installs load then onData. It does not parse constructor arguments.
        set_member("load", &xml_load);
        set_member("onData", &xml_ondata);
    }

    void xml_document::set_member(const tu_stringi& name, const as_value& value)
    {
        // SwiffPlayer.dll 0x100A8530: ignoreWhite uses normal Boolean conversion.
        if (std::strcmp(name.c_str(), "ignoreWhite") == 0) m_ignore_white = value.to_bool();
        else as_object::set_member(name, value);
    }

    bool xml_document::get_member(const tu_stringi& name, as_value* value)
    {
        // SwiffPlayer.dll 0x100A8BE0 tests the parser cursor, not status, for loaded.
        if (std::strcmp(name.c_str(), "loaded") == 0) value->set_bool(m_current != NULL);
        else if (std::strcmp(name.c_str(), "status") == 0) xml_set_int(value, m_status);
        else if (std::strcmp(name.c_str(), "ignoreWhite") == 0) value->set_bool(m_ignore_white);
        else return xml_node::get_member(name, value);
        return true;
    }

    void xml_document::clear_children()
    {
        // SwiffPlayer.dll 0x100A8400 clears children and restores the document cursor.
        m_first_child = NULL;
        m_current = this;
    }

    void xml_document::start_element(const char* name, const char** attributes)
    {
        // SwiffPlayer.dll 0x100A93E0 creates and links before reading attributes.
        m_current = new xml_node(name, 1, m_current);
        for (const char** attribute = attributes; *attribute; attribute += 2)
            m_current->m_attributes->set_member(*attribute, as_value(attribute[1]));
    }

    void xml_document::end_element(const char* name)
    {
        // SwiffPlayer.dll 0x100A8460 discards the closing name and follows parent.
        (void) name;
        m_current = m_current->m_parent;
    }

    void xml_document::character_data(const char* text, int length)
    {
        // SwiffPlayer.dll 0x100A92F0 always drops all-CR/LF/tab fragments.
        // With ignoreWhite it also drops all-space/CR/LF/tab fragments; it does
        // not trim mixed text, merge fragments, or discard spaces by default.
        int i = 0;
        while (i < length && (text[i] == '\n' || text[i] == '\r' || text[i] == '\t')) ++i;
        if (i == length) return;
        if (m_ignore_white)
        {
            i = 0;
            while (i < length && (text[i] == ' ' || text[i] == '\n' || text[i] == '\r' || text[i] == '\t')) ++i;
            if (i == length) return;
        }
        m_current = new xml_node(NULL, 3, m_current);
        const tu_string fragment(text, length);
        // [CHOSEN, not recovered] S3 bridge: String's native assignment API is
        // NUL-terminated. XML character data cannot contain a NUL byte.
        assign_dice_string_object_narrow(m_current->m_node_value.get_ptr(), fragment.c_str());
        m_current = m_current->m_parent;
    }

    void xml_ondata(const fn_call& fn)
    {
        // SwiffPlayer.dll 0x100A87F0 calls onLoad with argument-definedness;
        // there is no parse step here and the caller's result is untouched.
        as_value method;
        if (fn.this_ptr->get_member("onLoad", &method) && method.get_type() != as_value::UNDEFINED)
        {
            const as_value argument = fn.env->bottom(fn.first_arg_bottom_index);
            xml_call_method1(method, fn.env, fn.this_ptr, as_value(argument.get_type() != as_value::UNDEFINED));
        }
    }

    bool xml_document::parse(const char* text, int length)
    {
        // [CHOSEN, not recovered] S3 bridge factoring the parser sequence from
        // SwiffPlayer.dll 0x100A8610-0x100A8658. Retail passes isFinal=0 and
        // maps exactly result 1 to status 0, all other results to -1.
        dice_xml_parser* parser = dice_xml_parser::create(NULL);
        parser->set_element_handler(this);
        parser->set_character_data_handler(this);
        const int result = parser->parse(text, length, 0);
        m_status = result == 1 ? 0 : -1;
        parser->release();
        return result == 1;
    }

    void xml_load(const fn_call& fn)
    {
        // SwiffPlayer.dll 0x100A8570: clear first, synchronous rb read, no arity
        // guard or result assignment; open failure sends undefined to onData.
        xml_document* document = static_cast<xml_document*>(fn.this_ptr);
        IF_VERBOSE_ACTION(log_msg("-- load xml \n"));
        document->clear_children();
        const tu_string filename(fn.env->bottom(fn.first_arg_bottom_index).to_string());
        tu_file file(filename.c_str(), "rb");
        smart_ptr<as_object_interface> source;
        if (file.get_error() == TU_FILE_NO_ERROR)
        {
            file.go_to_end();
            const int length = file.get_position();
            file.set_position(0);
            array<char> bytes;
            bytes.resize(length + 1);
            file.read_bytes(&bytes[0], length);
            bytes[length] = 0;
            document->parse(&bytes[0], length);
            source = create_dice_string_object(NULL, 0);
            assign_dice_string_object_narrow(source.get_ptr(), &bytes[0]);
        }
        else document->m_status = -1;
        as_value method;
        if (document->get_member("onData", &method) && method.get_type() != as_value::UNDEFINED)
            xml_call_method1(method, fn.env, document,
                             source.get_ptr() ? as_value(source.get_ptr()) : as_value());
    }

    void xml_new(const fn_call& fn)
    {
        // SwiffPlayer.dll 0x1008B240 ignores all constructor arguments.
        fn.result->set_as_object_interface(new xml_document);
    }

    void xmlnode_new(const fn_call& fn)
    {
        // SwiffPlayer.dll 0x1008B200 ignores all constructor arguments.
        fn.result->set_as_object_interface(new xml_node);
    }

    void release_xml_script_references(as_object* object)
    {
        // [CHOSEN, not recovered] issue #7 / S3 bridge: the native registry
        // snapshot replaces retail's mark/clear traversal (0x100A9120). Clear
        // strong XML edges while that snapshot holds all referenced objects.
        xml_node* node = dynamic_cast<xml_node*>(object);
        if (!node) return;
        node->m_first_child = NULL;
        node->m_next_sibling = NULL;
        node->m_parent = NULL;
        node->m_previous_sibling = NULL;
        node->m_node_value = NULL;
        node->m_node_name = NULL;
        node->m_attributes = NULL;
        xml_document* document = dynamic_cast<xml_document*>(node);
        if (document) document->m_current = NULL;
    }
}
