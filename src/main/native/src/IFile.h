/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef IFILE_H_
#define IFILE_H_


#include "Checksum.h"
#include "Buffers.h"
#include "WritableUtils.h"
#include "PartitionIndex.h"
#include "MapOutputSpec.h"

namespace NativeTask {

/**
 * IFileReader
 */
class IFileReader {
private:
  InputStream * _stream;
  ChecksumInputStream * _source;
  ReadBuffer _reader;
  ChecksumType _checksumType;
  KeyValueType _kType;
  KeyValueType _vType;
  string _codec;
  int32_t _segmentIndex;
  IndexRange * _spillInfo;
  const char * _valuePos;
  uint32_t _valueLen;

public:
  IFileReader(InputStream * stream, ChecksumType checksumType,
               KeyValueType ktype, KeyValueType vtype,
               IndexRange * spill_infos, const string & codec);

  virtual ~IFileReader();

  /**
   * @return 0 if have next partition, none 0 if no more partition
   */
  int nextPartition();

  /**
   * get next key
   * NULL if no more, then next_partition() need to be called
   * NOTICE: before value() is called, the return pointer value is
   *         guaranteed to be valid
   */
  const char * nextKey(uint32_t & keyLen) {
    int64_t t1 = _reader.readVLong();
    int64_t t2 = _reader.readVLong();
    if (t1 == -1) {
      return NULL;
    }
    const char * kvbuff = _reader.get((uint32_t)(t1+t2));
    uint32_t len;
    switch (_kType) {
    case TextType:
      keyLen = WritableUtils::ReadVInt(kvbuff, len);
      break;
    case BytesType:
      keyLen = bswap(*(uint32_t*)kvbuff);
      len = 4;
      break;
    default:
      keyLen = t1;
      len = 0;
    }
    const char * kbuff = kvbuff + len;
    const char * vbuff = kbuff + keyLen;
    switch (_vType) {
    case TextType:
      _valueLen = WritableUtils::ReadVInt(vbuff, len);
      _valuePos = vbuff + len;
      break;
    case BytesType:
      _valueLen = bswap(*(uint32_t*)vbuff);
      _valuePos = vbuff + 4;
      break;
    default:
      _valueLen = t2;
      _valuePos = vbuff;
    }
    return kbuff;
  }

  /**
   * length of current value part of IFile entry
   */
  const uint32_t valueLen() {
    return _valueLen;
  }

  /**
   * get current value
   */
  const char * value(uint32_t & valueLen) {
    valueLen = _valueLen;
    return _valuePos;
  }
};

/**
 * IFile Writer
 */
class IFileWriter : public Collector {
protected:
  OutputStream * _stream;
  ChecksumOutputStream * _dest;
  ChecksumType _checksumType;
  KeyValueType _kType;
  KeyValueType _vType;
  string       _codec;
  AppendBuffer _appendBuffer;
  vector<IndexEntry> _spillInfo;

public:
  IFileWriter(OutputStream * stream, ChecksumType checksumType,
               KeyValueType ktype, KeyValueType vtype, const string & codec);

  virtual ~IFileWriter();

  void startPartition();

  void endPartition();

  void writeKey(const char * key, uint32_t keyLen, uint32_t valueLen);

  void writeValue(const char * value, uint32_t valueLen);

  void write(const char * key, uint32_t keyLen, const char * value,
             uint32_t valueLen) {
    writeKey(key, keyLen, valueLen);
    writeValue(value, valueLen);
  }

  IndexRange * getIndex(uint32_t start);

  void getStatistics(uint64_t & offset, uint64_t & realOffset);

  virtual void collect(const void * key, uint32_t keyLen,
                       const void * value, uint32_t valueLen) {
    write((const char*)key, keyLen, (const char*)value, valueLen);
  }
};



} // namespace NativeTask



#endif /* IFILE_H_ */
