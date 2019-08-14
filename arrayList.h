#pragma once
typedef int m_int;

class CarrayList
{
public:
	CarrayList::CarrayList(int arrayLength);
	CarrayList::~CarrayList();
public:
	void clear();
	bool isEmpty();
	int length();
	int get(int i);
	void insert(int i, m_int x);
	void add(m_int m_element);
	void remove(int i);
	int indexOf(m_int x);
private:
	m_int* element;
	int arrayLength{0};
	int listSize{0};
};

