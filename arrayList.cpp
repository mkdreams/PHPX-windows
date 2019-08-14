#include "arrayList.h"
#include <string>

using namespace std;

void CarrayList::clear() {
	listSize = 0;
}

CarrayList::CarrayList(int length) {
	arrayLength = length;
	element = new m_int[length];
}

bool CarrayList::isEmpty() {
	if (listSize == 0) {
		return true;
	}
	else {
		return false;
	}
}

int CarrayList::length() {
	return listSize;
}

int CarrayList::get(int i) {
	return element[i];
}

int CarrayList::indexOf(m_int m_element) {
	int index = (int) (find(element, element + listSize, m_element) - element);

	if (index == listSize)
		return -1;
	return index;
}

void CarrayList::add( m_int m_element) {
	listSize++;
	element[listSize] = m_element;
}

void CarrayList::insert(int i,m_int m_element) {
	//往后移动一个
	copy_backward(element + i, element + listSize, element + listSize + 1);
	element[i] = m_element;
	listSize++;
}

void CarrayList::remove(int i) {
	copy(element + i + 1, element + listSize, element + i);
	listSize--;
}


CarrayList::~CarrayList() {
	delete[] element;
}



