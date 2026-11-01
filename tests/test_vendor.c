/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The names of the members of a container, and the files of vendors. */

#include "allocator.h"
#include "check.h"
#include "members.h"
#include "vendor.h"

static void path_is(const char *link, const char *expected)
{
    char *path = NULL;
    const x3pio_status status =
        x3pio_member_path(x3pio_default_allocator(), link, strlen(link), &path);
    if (expected == NULL)
    {
        CHECK(status == X3PIO_ERR_FORMAT && path == NULL);
    }
    else
    {
        CHECK_OK(status);
        CHECK_STRING(path, expected);
    }
    x3pio_release(x3pio_default_allocator(), path);
}

static void member_is(const char *uri, const char *expected)
{
    char *member = NULL;
    const x3pio_status status = x3pio_vendor_member(x3pio_default_allocator(),
                                                    uri, strlen(uri), &member);
    if (expected == NULL)
    {
        CHECK(status == X3PIO_ERR_ARGUMENT && member == NULL);
    }
    else
    {
        CHECK_OK(status);
        CHECK_STRING(member, expected);
    }
    x3pio_release(x3pio_default_allocator(), member);
}

static void test_links(void)
{
    path_is("bindata/data.bin", "bindata/data.bin");
    path_is("bindata\\data.bin", "bindata/data.bin");
    path_is("./bindata//data.bin", "bindata/data.bin");
    path_is("  main.xml\n", "main.xml");
    path_is("a/./b/../../c", NULL);
    path_is("../main.xml", NULL);
    path_is("bindata/../../x", NULL);
    path_is("/etc/passwd", NULL);
    path_is("\\windows", NULL);
    path_is("C:\\x.bin", NULL);
    path_is("c:data.bin", NULL);
    path_is("http://host/x", NULL);
    path_is("file:///x", NULL);
    path_is("", NULL);
    path_is("   ", NULL);
    path_is("./", NULL);
    path_is("a.b:c/x", NULL); /* a.b is a scheme */
    /* A colon after a slash is a name, not a scheme. */
    path_is("dir/a:b", "dir/a:b");
}

static void test_reserved(void)
{
    CHECK(x3pio_is_reserved_name("main.xml"));
    CHECK(x3pio_is_reserved_name("md5checksum.hex"));
    CHECK(x3pio_is_reserved_name("bindata/data.bin"));
    CHECK(x3pio_is_reserved_name("bindata/valid.bin"));
    CHECK(!x3pio_is_reserved_name("bindata/other.bin"));
    CHECK(!x3pio_is_reserved_name("Main.xml"));
}

static void test_vendor_below(void)
{
    /* Whole segments, and the scheme does not count. */
    CHECK(x3pio_vendor_below("http://a.b", "http://a.b/c/d.xml"));
    CHECK(x3pio_vendor_below("https://a.b", "http://a.b/c/d.xml"));
    CHECK(x3pio_vendor_below("a.b", "http://a.b/c/d.xml"));
    CHECK(x3pio_vendor_below("http://a.b/c", "http://a.b/c/d.xml"));
    CHECK(x3pio_vendor_below("http://a.b/c/", "http://a.b/c/d.xml"));
    CHECK(x3pio_vendor_below("http://a.b/", "http://a.b/c"));
    CHECK(x3pio_vendor_below("http://a.b/c/d.xml", "http://a.b/c/d.xml"));
    CHECK(!x3pio_vendor_below("http://a.b/c", "http://a.b/cd/e"));
    CHECK(!x3pio_vendor_below("http://a.b/c/d.xml", "http://a.b/c"));
    CHECK(!x3pio_vendor_below("http://a.b", "http://a.bc/x"));
    CHECK(!x3pio_vendor_below("http://x.y", "http://a.b/c"));
    /* What is no URI of a domain name matches nothing. */
    CHECK(!x3pio_vendor_below("http:///c", "http://a.b/c"));
    CHECK(!x3pio_vendor_below("http://a..b", "http://a..b/c"));
    CHECK(!x3pio_vendor_below("http://a.b/c//d", "http://a.b/c/d"));
    CHECK(!x3pio_vendor_below("http://a.b/../c", "http://a.b/c"));
    CHECK(!x3pio_vendor_below("http://a.b", "not a uri\\x"));
    CHECK(x3pio_vendor_prefix_valid("http://a.b"));
    CHECK(x3pio_vendor_prefix_valid("a.b/c/"));
    CHECK(!x3pio_vendor_prefix_valid(""));
    CHECK(!x3pio_vendor_prefix_valid("http://a.b/c?x"));
}

static void test_vendor_members(void)
{
    member_is("http://www.vendor.com/my/a.xml", "www\\vendor\\com\\my\\a.xml");
    member_is("https://example.org/f", "example\\org\\f");
    member_is("//host.net/dir/file.bin", "host\\net\\dir\\file.bin");
    member_is("  http://a.b/c  ", "a\\b\\c");
    member_is("http://host", NULL);
    member_is("http://host/", NULL);
    member_is("http:///path", NULL);
    member_is("http://host//a", NULL);
    member_is("http://host/a/../b", NULL);
    member_is("http://host/./b", NULL);
    member_is("http://ho..st/b", NULL);
    member_is("http://.host/b", NULL);
    member_is("http://host/a?q=1", NULL);
    member_is("http://host/a#x", NULL);
    member_is("http://host\\a", NULL);
    member_is("not a uri", NULL);
    member_is("", NULL);
}

static x3pio_status build_index(x3pio_member_index *index,
                                const char *const *names, size_t count,
                                const char **message)
{
    char *list[16];
    char room[16][64];
    for (size_t i = 0; i < count; ++i)
    {
        snprintf(room[i], sizeof room[i], "%s", names[i]);
        list[i] = room[i];
    }
    return x3pio_member_index_build(index, x3pio_default_allocator(), list,
                                    count, message);
}

static void test_index(void)
{
    const char *message = "";
    x3pio_member_index index;
    const char *plain[] = {"main.xml",
                           "md5checksum.hex",
                           "bindata\\data.bin",
                           "./bindata/valid.bin",
                           "folder/",
                           "__MACOSX/._main.xml",
                           "bindata/._data.bin",
                           ".DS_Store",
                           "Thumbs.db"};
    CHECK_OK(build_index(&index, plain, 9, &message));
    CHECK_STRING(index.main_stored, "main.xml");
    CHECK_STRING(x3pio_member_index_find(&index, "bindata/data.bin"),
                 "bindata\\data.bin");
    CHECK_STRING(x3pio_member_index_find(&index, "bindata/valid.bin"),
                 "./bindata/valid.bin");
    CHECK(x3pio_member_index_find(&index, "folder") == NULL);
    CHECK(x3pio_member_index_find(&index, ".DS_Store") == NULL);
    CHECK(x3pio_member_index_find(&index, "bindata/._data.bin") == NULL);
    CHECK(x3pio_member_index_find(&index, "Main.xml") == NULL);
    x3pio_member_index_free(&index);

    /* Everything in one folder. */
    const char *folder[] = {"scan/main.xml", "scan/bindata/data.bin",
                            "scan/md5checksum.hex"};
    CHECK_OK(build_index(&index, folder, 3, &message));
    CHECK_STRING(index.main_stored, "scan/main.xml");
    CHECK_STRING(index.prefix, "scan/");
    CHECK_STRING(x3pio_member_index_find(&index, "bindata/data.bin"),
                 "scan/bindata/data.bin");
    x3pio_member_index_free(&index);

    /* A main.xml beside the folder wins. */
    const char *both[] = {"main.xml", "scan/main.xml"};
    CHECK_OK(build_index(&index, both, 2, &message));
    CHECK_STRING(index.main_stored, "main.xml");
    x3pio_member_index_free(&index);

    const char *none[] = {"data.bin", "a/b/main.xml"};
    CHECK_STATUS(X3PIO_ERR_FORMAT, build_index(&index, none, 2, &message));
    CHECK_STRING(message, "The container has no main.xml");
    const char *two[] = {"a/main.xml", "b/main.xml"};
    CHECK_STATUS(X3PIO_ERR_FORMAT, build_index(&index, two, 2, &message));
    CHECK(strstr(message, "several folders") != NULL);
    CHECK_STATUS(X3PIO_ERR_FORMAT, build_index(&index, none, 0, &message));
}

int main(void)
{
    test_links();
    test_reserved();
    test_vendor_members();
    test_vendor_below();
    test_index();
    return finish_tests();
}
