#ifndef WRITERBASE_HPP
#define WRITERBASE_HPP

#include <map>
#include <set>
#include <string>


class WriterBase
{
public:
    virtual bool IsInline(void) const = 0;
    virtual bool IsExternal(void) const = 0;
};


typedef std::map<std::string, std::string> ConstantMap;
typedef std::set<std::string> IncludeSet;


#endif
