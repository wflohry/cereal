/*! \file named_binary.hpp
    \brief Named binary input and output archives */
/*
  Copyright (c) 2014, Randolph Voorhies, Shane Grant
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of cereal nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL RANDOLPH VOORHIES OR SHANE GRANT BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef CEREAL_ARCHIVES_NAMED_BINARY_HPP_
#define CEREAL_ARCHIVES_NAMED_BINARY_HPP_

#include "cereal/cereal.hpp"
#include <sstream>
#include <stack>

namespace cereal
{
  // ######################################################################
  //! An output archive designed to save data in a compact binary representation
  /*! This archive outputs data to a stream in an extremely compact binary
      representation with as little extra metadata as possible.

      This archive does nothing to ensure that the endianness of the saved
      and loaded data is the same.  If you need to have portability over
      architectures with different endianness, use PortableNamedBinaryOutputArchive.

      When using a binary archive and a file stream, you must use the
      std::ios::binary format flag to avoid having your data altered
      inadvertently.

      \ingroup Archives */
  class NamedBinaryOutputArchive : public OutputArchive<NamedBinaryOutputArchive, AllowEmptyClassElision>
  {
    public:
      struct NamedValue_t
      {
          std::stringstream itsStream;
          std::size_t itsSize = 0;
          std::string itsName = "";
          bool written = false;
      };


      //! Construct, outputting to the provided stream
      /*! @param stream The stream to output to.  Can be a stringstream, a file stream, or
                        even cout! */
      NamedBinaryOutputArchive(std::ostream & stream) :
        OutputArchive<NamedBinaryOutputArchive, AllowEmptyClassElision>(this),
        itsStream(stream)
      { }

      ~NamedBinaryOutputArchive() CEREAL_NOEXCEPT = default;

      //! Writes size bytes of data to the output stream
      void saveBinary( const void * data, std::size_t size)
      {
          auto &itsValue = itsNodes.top();
          itsValue.itsName = nextName;
          nextName = "";
          std::stringstream ss;
          itsValue.itsSize = static_cast<std::size_t>( itsValue.itsStream.rdbuf()->sputn( reinterpret_cast<const char*>(data), size ) );
          if (itsValue.itsSize != size){
              throw Exception("Failed to write " + std::to_string(size) + " bytes to stringstream! Wrote " + std::to_string(itsValue.itsSize));
          }
          itsValue.itsStream.seekg(0, std::ios::beg);
      }

      void finishNode()
      {
          auto &node = itsNodes.top();
          if (node.itsSize == 0){
              itsNodes.pop();
              return;
          }
          const uint64_t totalSize = node.itsSize + node.itsName.size() + sizeof(std::size_t);
          for (size_t i = 0; i < sizeof(uint64_t); ++i){
            itsStream.put((totalSize >> 8*i) & 0xF);
          }
          for (size_t i = 0; i < sizeof(uint64_t); ++i){
            itsStream.put((node.itsName.size() >> 8*i) & 0xF);
          }
          itsStream.write(&node.itsName[0], node.itsName.size());
          for (size_t i = 0; i < sizeof(uint64_t); ++i){
            itsStream.put((node.itsSize >> 8*i) & 0xF);
          }

          std::string value_str = node.itsStream.str();
          const int writtenSize = itsStream.rdbuf()->sputn(&value_str[0], std::min(value_str.size(), node.itsSize));
          if(writtenSize != node.itsSize){
              node.itsStream.seekg(0, std::ios::beg);
              const size_t avail = node.itsStream.rdbuf()->in_avail();
              throw Exception("Failed to write " + std::to_string(node.itsSize) + " bytes to output stream! Wrote " + std::to_string(writtenSize));
          }
          itsNodes.pop();
      }

      void addNode()
      {
          NamedValue_t blankNode;
          blankNode.itsSize = 0;
          itsNodes.push(std::move(blankNode));
      }

      void setNextName(const std::string &name){
       nextName = name;
      }

    private:
      std::string nextName;
      std::stack<NamedValue_t> itsNodes;
      std::ostream & itsStream;
  };

  template <typename T> inline
  void prologue( NamedBinaryOutputArchive &ar, T const &value)
  {
    ar.addNode();
  }

  template <typename T> inline
  void epilogue( NamedBinaryOutputArchive &ar, T const &value)
  {
    ar.finishNode();
  }

  // ######################################################################
  //! An input archive designed to load data saved using NamedBinaryOutputArchive
  /*  This archive does nothing to ensure that the endianness of the saved
      and loaded data is the same.  If you need to have portability over
      architectures with different endianness, use PortableNamedBinaryOutputArchive.

      When using a binary archive and a file stream, you must use the
      std::ios::binary format flag to avoid having your data altered
      inadvertently.

      \ingroup Archives */
  class NamedBinaryInputArchive : public InputArchive<NamedBinaryInputArchive, AllowEmptyClassElision>
  {
    public:
      //! Construct, loading from the provided stream
      NamedBinaryInputArchive(std::istream & stream) :
        InputArchive<NamedBinaryInputArchive, AllowEmptyClassElision>(this),
        itsStream(stream)
      {

      }

      ~NamedBinaryInputArchive() CEREAL_NOEXCEPT = default;

      //! Reads size bytes of data from the input stream
      void loadBinary( void * const data, std::size_t size )
      {
        auto const readSize = static_cast<std::size_t>( itsStream.rdbuf()->sgetn( reinterpret_cast<char*>( data ), size ) );

        if(readSize != size)
          throw Exception("Failed to read " + std::to_string(size) + " bytes from input stream! Read " + std::to_string(readSize));
      }

      void setNextName(const std::string &name){

      }

    private:
      std::istream & itsStream;
  };

  // ######################################################################
  // Common BinaryArchive serialization functions

  //! Saving for POD types to binary
  template<class T> inline
  typename std::enable_if<std::is_arithmetic<T>::value, void>::type
  CEREAL_SAVE_FUNCTION_NAME(NamedBinaryOutputArchive & ar, T const & t)
  {
    ar.saveBinary(std::addressof(t), sizeof(t));
  }

  //! Loading for POD types from binary
  template<class T> inline
  typename std::enable_if<std::is_arithmetic<T>::value, void>::type
  CEREAL_LOAD_FUNCTION_NAME(NamedBinaryInputArchive & ar, T & t)
  {
    ar.loadBinary(std::addressof(t), sizeof(t));
  }

  //! Serializing NVP types to binary
  template <class Archive, class T> inline
  CEREAL_ARCHIVE_RESTRICT(NamedBinaryInputArchive, NamedBinaryOutputArchive)
  CEREAL_SERIALIZE_FUNCTION_NAME( Archive & ar, NameValuePair<T> & t )
  {
    ar.setNextName(t.name);
    ar( t.value );
  }

  //! Serializing SizeTags to binary
  template <class Archive, class T> inline
  CEREAL_ARCHIVE_RESTRICT(NamedBinaryInputArchive, NamedBinaryOutputArchive)
  CEREAL_SERIALIZE_FUNCTION_NAME( Archive & ar, SizeTag<T> & t )
  {
    ar( t.size );
  }

  //! Saving binary data
  template <class T> inline
  void CEREAL_SAVE_FUNCTION_NAME(NamedBinaryOutputArchive & ar, BinaryData<T> const & bd)
  {
    ar.saveBinary( bd.data, static_cast<std::size_t>( bd.size ) );
  }

  //! Loading binary data
  template <class T> inline
  void CEREAL_LOAD_FUNCTION_NAME(NamedBinaryInputArchive & ar, BinaryData<T> & bd)
  {
    ar.loadBinary(bd.data, static_cast<std::size_t>(bd.size));
  }
} // namespace cereal

// register archives for polymorphic support
CEREAL_REGISTER_ARCHIVE(cereal::NamedBinaryOutputArchive)
CEREAL_REGISTER_ARCHIVE(cereal::NamedBinaryInputArchive)

// tie input and output archives together
CEREAL_SETUP_ARCHIVE_TRAITS(cereal::NamedBinaryInputArchive, cereal::NamedBinaryOutputArchive)

#endif // CEREAL_ARCHIVES_BINARY_HPP_
