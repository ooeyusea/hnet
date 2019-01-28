#include "dataset.h"

DataSet::DataSet() {

}

DataSet::~DataSet() {

}

bool DataSet::Load(const char * path) {
	return true;
}

void DataSet::Propose(int64_t zxId, std::string data) {

}

void DataSet::Commit(int64_t zxId) {

}

void DataSet::Snap(std::string data) {

}

void DataSet::Trunc(int64_t zxId) {

}

void DataSet::Flush() {

}
