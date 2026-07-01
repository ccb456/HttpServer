#include "net/Buffer.h"
#include <iostream>
#include <cassert>
#include <string.h>

using namespace ccb;

void test_basic()
{
    Buffer buf;

    buf.append("hello", 5);

    assert(buf.readableBytes() == 5);
    assert(std::string(buf.peek(), 5) == "hello");

    buf.retrieve(2);

    assert(buf.readableBytes() == 3);
    assert(std::string(buf.peek(), 3) == "llo");

    std::cout << "test_basic passed\n";
}

void test_retrieve_all()
{
    Buffer buf;

    buf.append("hello world", 11);

    buf.retrieveAll();

    assert(buf.readableBytes() == 0);
    assert(buf.writableBytes() > 0);

    std::cout << "test_retrieve_all passed\n";
}

void test_expand()
{
    Buffer buf;

    std::string big(100000, 'x');

    buf.append(big);

    assert(buf.readableBytes() == 100000);
    assert(buf.peek()[0] == 'x');

    std::cout << "test_expand passed\n";
}

void test_make_space()
{
    Buffer buf;

    buf.append(std::string(1000, 'a'));

    buf.retrieve(900); // 制造 prepend space

    buf.append(std::string(500, 'b'));

    assert(buf.readableBytes() == 600);

    std::cout << "test_make_space passed\n";
}

void test_append_buffer()
{
    Buffer buf1;
    Buffer buf2;

    buf1.append("hello", 5);
    buf2.append(buf1);

    assert(buf2.readableBytes() == 5);
    assert(std::string(buf2.peek(), 5) == "hello");

    std::cout << "test_append_buffer passed\n";
}

void test_to_string()
{
    Buffer buf;

    buf.append("hello", 5);

    std::string s = buf.retrieveAllToString();

    assert(s == "hello");
    assert(buf.readableBytes() == 0);

    std::cout << "test_to_string passed\n";
}

#include <unistd.h>

void test_read_fd()
{
    int fds[2];
    pipe(fds);

    const char* msg = "hello socket";
    write(fds[1], msg, strlen(msg));

    Buffer buf;
    int savedErrno = 0;

    ssize_t n = buf.readFd(fds[0], &savedErrno);

    assert(n > 0);
    assert(buf.readableBytes() == strlen(msg));
    assert(std::string(buf.peek(), buf.readableBytes()) == msg);

    std::cout << "test_read_fd passed\n";

    close(fds[0]);
    close(fds[1]);
}

void test_write_fd()
{
    int fds[2];
    pipe(fds);

    Buffer buf;
    buf.append("hello", 5);

    int savedErrno = 0;
    buf.writeFd(fds[1], &savedErrno);

    char tmp[10] = {0};
    read(fds[0], tmp, 5);

    assert(std::string(tmp, 5) == "hello");

    std::cout << "test_write_fd passed\n";

    close(fds[0]);
    close(fds[1]);
}

int main()
{
    test_basic();
    test_retrieve_all();
    test_expand();
    test_make_space();
    test_append_buffer();
    test_to_string();
    test_read_fd();
    test_write_fd();

    std::cout << "ALL TESTS PASSED\n";
}