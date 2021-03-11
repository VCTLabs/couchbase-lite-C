//
// Document.hh
//
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once
#include "Database.hh"
#include "CBLDocument.h"
#include "fleece/Mutable.hh"
#include <string>

// PLEASE NOTE: This C++ wrapper API is provided as a convenience only.
// It is not considered part of the official Couchbase Lite API.

namespace cbl {
    class MutableDocument;

    class Document : protected RefCounted {
    public:
        // Metadata:

        slice id() const                                {return CBLDocument_ID(ref());}

        slice revisionID() const                        {return CBLDocument_RevisionID(ref());}

        uint64_t sequence() const                       {return CBLDocument_Sequence(ref());}

        // Properties:

        fleece::Dict properties() const                 {return CBLDocument_Properties(ref());}

        std::string propertiesAsJSON() const {
            alloc_slice json(CBLDocument_CreateJSON(ref()));
            return std::string(json);
        }

        fleece::Value operator[] (slice key) const {return properties()[key];}

        // Operations:

        inline MutableDocument mutableCopy() const;

    protected:
        Document(CBLRefCounted* r)                      :RefCounted(r) { }

        static Document adopt(const CBLDocument *d, CBLError *error) {
            if (!d && error->code != 0)
                throw *error;
            Document doc;
            doc._ref = (CBLRefCounted*)d;
            return doc;
        }

        static bool checkSave(bool saveResult, CBLError &error) {
            if (saveResult)
                return true;
            else if (error.code == CBLErrorConflict && error.domain == CBLDomain)
                return false;
            else
                throw error;
        }

        friend class Database;
        friend class Replicator;

        CBL_REFCOUNTED_BOILERPLATE(Document, RefCounted, const CBLDocument)
    };


    class MutableDocument : public Document {
    public:
        explicit MutableDocument(nullptr_t)             {_ref = (CBLRefCounted*)CBLDocument_CreateWithID(fleece::nullslice);}
        explicit MutableDocument(slice docID)     {_ref = (CBLRefCounted*)CBLDocument_CreateWithID(docID);}

        fleece::MutableDict properties()                {return CBLDocument_MutableProperties(ref());}

        template <typename V>
        void set(slice key, const V &val)               {properties().set(key, val);}
        template <typename K, typename V>
        void set(const K &key, const V &val)            {properties().set(key, val);}

        fleece::keyref<fleece::MutableDict,fleece::slice> operator[] (slice key)
                                                        {return properties()[key];}

        void setProperties(fleece::MutableDict properties) {
            CBLDocument_SetProperties(ref(), properties);
        }

        void setProperties(fleece::Dict properties) {
            CBLDocument_SetProperties(ref(), properties.mutableCopy());
        }

        void setPropertiesAsJSON(slice json) {
            CBLError error;
            if (!CBLDocument_SetJSON(ref(), json, &error))
                throw error;
        }

    protected:
        static MutableDocument adopt(CBLDocument *d, CBLError *error) {
            if (!d && error->code != 0)
                throw *error;            
            MutableDocument doc;
            doc._ref = (CBLRefCounted*)d;
            return doc;
        }

        friend class Database;
        friend class Document;
        CBL_REFCOUNTED_BOILERPLATE(MutableDocument, Document, CBLDocument)
    };


    // Database method bodies:

    inline Document Database::getDocument(slice id) const {
        CBLError error;
        return Document::adopt(CBLDatabase_GetDocument(ref(), id, &error), &error);
    }

    inline MutableDocument Database::getMutableDocument(slice id) const {
        CBLError error;
        return MutableDocument::adopt(CBLDatabase_GetMutableDocument(ref(), id, &error), &error);
    }


    inline void Database::saveDocument(MutableDocument &doc) {
        (void) saveDocument(doc, kCBLConcurrencyControlLastWriteWins);
    }


    inline bool Database::saveDocument(MutableDocument &doc, CBLConcurrencyControl c) {
        CBLError error;
        return Document::checkSave(
            CBLDatabase_SaveDocumentWithConcurrencyControl(ref(), doc.ref(), c, &error),
            error);
    }


    inline bool Database::saveDocument(MutableDocument &doc,
                                       SaveConflictHandler conflictHandler)
    {
        CBLConflictHandler cHandler = [](void *context, CBLDocument *myDoc,
                                             const CBLDocument *otherDoc) -> bool {
            return (*(SaveConflictHandler*)context)(MutableDocument(myDoc),
                                                    Document(otherDoc));
        };
        CBLError error;
        return Document::checkSave(
            CBLDatabase_SaveDocumentWithConflictHandler(ref(), doc.ref(), cHandler, &conflictHandler, &error),
            error);
    }

    inline bool Database::deleteDocument(Document &doc, CBLConcurrencyControl cc) {
        CBLError error;
        return Document::checkSave(CBLDatabase_DeleteDocumentWithConcurrencyControl(
                                                                    ref(), doc.ref(), cc, &error),
                                   error);
    }

    inline void Database::purgeDocument(Document &doc) {
        CBLError error;
        check(CBLDatabase_PurgeDocument(ref(), doc.ref(), &error), error);
    }


    inline MutableDocument Document::mutableCopy() const {
        return MutableDocument::adopt(CBLDocument_MutableCopy(ref()), nullptr);
    }


}
