#pragma once

#include <vector>
#include "ngx_global.h" 
using namespace std;

class CConfig
{
private:
	CConfig();

public:
	~CConfig();

private:
	static CConfig *m_instance;

public:
	static CConfig *GetInstance()
	{
		if (m_instance == NULL)
		{
			// 锁（本来该加锁，但在主线程中率先执行了GetInstance函数，这样就不存在多线程调用该函数导致的需要互斥的问题，因此这里就没有实际加锁）
			if (m_instance == NULL)
			{
				m_instance = new CConfig();
				static CGarhuishou cl; // 静态局部对象，~CConfig()调用前会调用~CGarhuishou()
			}
			// 放锁
		}
		return m_instance;
	}
	class CGarhuishou
	{ // 类中套类，用于释放对象
	public:
		~CGarhuishou()
		{
			if (CConfig::m_instance)
			{
				delete CConfig::m_instance;
				CConfig::m_instance = NULL;
			}
		}
	};

public:
	bool Load(const char *pconfName); // 装载配置文件
	const char *GetString(const char *p_itemname);
	int GetIntDefault(const char *p_itemname, const int def);

public:
	vector<LPCConfItem> m_ConfigItemList; // 存储配置信息的列表
};
