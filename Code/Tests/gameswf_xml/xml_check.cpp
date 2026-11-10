// [TEMP] [CHOSEN, not recovered] Native gameswf XML parser integration fixture.
// Uses the production objects/parser, no mocked engine behavior or game content.
#include "gameswf/gameswf_xml.h"
#include <cstdio>
#include <cstring>

static int failures;
static void check(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL %s\n", message); ++failures; }
}

static bool member_text(gameswf::as_object_interface* object, const char* name, const char* expected)
{
    gameswf::as_value value;
    return object && object->get_member(name, &value)
        && std::strcmp(value.to_string(), expected) == 0;
}

static int callback_count;
static bool callback_defined;
static bool callback_object;
static gameswf::as_object_interface* callback_receiver;
static tu_string callback_source;

static void on_data(const gameswf::fn_call& fn)
{
    ++callback_count;
    callback_receiver = fn.this_ptr;
    callback_defined = fn.arg(0).get_type() != gameswf::as_value::UNDEFINED;
    callback_object = fn.arg(0).get_type() == gameswf::as_value::OBJECT;
    callback_source = callback_defined ? fn.arg(0).to_string() : "";
}

static void check_tree(gameswf::xml_document* document)
{
    using namespace gameswf;
    check(document->m_status == 0, "parse status");
    check(document->m_current == document, "closed document cursor");
    xml_node* root = document->m_first_child.get_ptr();
    check(root && root->m_node_type == 1 && root->m_parent == document, "root links/type");
    check(member_text(root, "nodeName", "root"), "root name");
    if (!root) return;
    check(member_text(root->m_attributes.get_ptr(), "id", "7"), "attribute");
    xml_node* text = root->m_first_child.get_ptr();
    check(text && text->m_node_type == 3, "text node type");
    check(member_text(text, "nodeValue", "text"), "text node value");
    if (!text) return;
    xml_node* child = text->m_next_sibling.get_ptr();
    check(member_text(child, "nodeName", "child"), "child name");
    check(child && child->m_parent == root && child->m_previous_sibling == text, "child links");
}

int main(int argc, char** argv)
{
    using namespace gameswf;
    if (argc != 2) { std::fprintf(stderr, "usage: gameswf_xml_check scratch-file\n"); return 2; }
    const char xml[] = "<root id=\"7\">text<child /></root>";
    smart_ptr<xml_document> document = new xml_document;
    check(document->parse(xml, int(sizeof(xml) - 1)), "tiny string accepted");
    check_tree(document.get_ptr());

    document->clear_children();
    const char malformed[] = "<root></wrong>";
    check(!document->parse(malformed, int(sizeof(malformed) - 1)), "mismatched tag rejected");
    check(document->m_status == -1, "error status");
    document->clear_children();
    const char incomplete[] = "<root>";
    check(document->parse(incomplete, int(sizeof(incomplete) - 1)), "retail non-final input accepted");
    check(document->m_current != document.get_ptr(), "incomplete cursor retained");

    std::FILE* file = std::fopen(argv[1], "wb");
    if (!file) { std::perror("fixture XML file"); return 2; }
    const bool written = std::fwrite(xml, 1, sizeof(xml) - 1, file) == sizeof(xml) - 1;
    const bool closed = std::fclose(file) == 0;
    if (!written || !closed) return 2;

    document->set_member("onData", &on_data);
    as_environment env;
    env.push(as_value(argv[1]));
    as_value result("unchanged");
    fn_call load(&result, document.get_ptr(), &env, 1, env.get_top_index());
    xml_load(load);
    check_tree(document.get_ptr());
    check(callback_count == 1 && callback_defined && callback_object, "onData receives String object");
    check(callback_source == xml && callback_receiver == document.get_ptr(), "onData source and receiver");
    check(std::strcmp(result.to_string(), "unchanged") == 0, "load leaves result untouched");
    file = std::fopen(argv[1], "wb");
    if (!file) return 2;
    const bool malformed_written = std::fwrite(malformed, 1, sizeof(malformed) - 1, file) == sizeof(malformed) - 1;
    const bool malformed_closed = std::fclose(file) == 0;
    if (!malformed_written || !malformed_closed) return 2;
    xml_load(load);
    check(document->m_status == -1, "file parse error status");
    check(callback_count == 2 && callback_defined && callback_object && callback_source == malformed,
          "parse error still sends source String to onData");
    std::remove(argv[1]);
    xml_load(load);
    check(callback_count == 3 && !callback_defined, "missing file sends undefined");
    check(document->m_status == -1 && !document->m_first_child.get_ptr(), "missing file clears tree/status");
    check(document->m_current == document.get_ptr(), "missing file resets cursor");
    std::printf("gameswf_xml_check: %d failures\n", failures);
    return failures ? 1 : 0;
}
