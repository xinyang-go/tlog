#pragma once

#ifndef _TLOG_HPP_
#define _TLOG_HPP_

#include <iostream>
#include <fstream>
#include <thread>

namespace tlog {

    class tbuf : public std::streambuf {
    public:
        tbuf(std::string name);

        ~tbuf();

    protected:
        virtual std::streamsize xsputn(const char *s, std::streamsize size) override;

        virtual std::streamsize xsgetn(char *s, std::streamsize size) override;

        virtual int underflow() override;

        virtual int overflow(int c) override;

    private:
        int _fd[2];
        char buf[80];
        int child_pid;

        std::string name;
    };

    // extern thread_local std::istream tin;
    // extern thread_local std::ostream tout;
}

#endif
