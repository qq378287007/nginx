#include <iostream>
#include <fstream>
#include <algorithm>
#include <functional>
#include <iterator>
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;

static void trim(string &str)
{
    static string blanks("\f\v\r\t\n ");
    str.erase(str.find_last_not_of(blanks) + 1);
    str.erase(0, str.find_first_not_of(blanks));
}
static bool startsWith(const string &s, const string &sub)
{
    return s.find(sub) == 0;
}
static bool endsWith(const string &s, const string &sub)
{
    return s.rfind(sub) == s.length() - sub.length();
}

// 字符串分割
static void stringSplit(const string &str, const string &split, vector<string> &res)
{
    res.clear();
    if (str.empty())
        return;

    string strs = str + split;
    for (size_t pos = strs.find(split); pos != strs.npos; pos = strs.find(split))
    {
        string tmp = strs.substr(0, pos);
        trim(tmp); // 去掉前后空格
        res.push_back(tmp);
        strs = strs.substr(pos + split.size());
    }
}

unordered_map<string, string> configMap;

// 装载配置文件
static bool Load(const string &fileName)
{
    ifstream in;
    in.open(fileName);
    if (!in)
        return false;

    string line;
    while (getline(in, line))
    {
        trim(line);
        if (line.empty())
            continue;

        if (startsWith(line, "#") || startsWith(line, "[")) // 注释行
            continue;

        vector<string> res;
        stringSplit(line, "=", res); // 字符串分割

        if (res.size() == 2)
            configMap.emplace(res[0], res[1]);
    }
    in.close();

    return true;
}
