/* -*- C++ -*- */
// Offer_Database.h,v 1.13 2000/03/23 20:52:34 nanbor Exp

// ========================================================================
//
// = LIBRARY
//    orbsvcs
//
// = FILENAME
//    Offer_Database.h
//
// = AUTHOR
//    Seth Widoff <sbw1@cs.wustl.edu>
//
// ========================================================================

#ifndef TAO_OFFER_DATABASE_H
#define TAO_OFFER_DATABASE_H
#include "ace/pre.h"

#include "Trader.h"
#include "Offer_Iterators.h"

template <class LOCK_TYPE> class TAO_Service_Offer_Iterator;

template <class LOCK_TYPE>
class TAO_Offer_Database
// = DESCRIPTION
//   The TAO_Offer_Database encapsulates the mapping of service
//   types to those offers exported with that service types. The
//   underlying structure is a map of maps. The first maps maps the
//   service type name to a map of exported offers. The second map
//   maps the identifying index for that offer within the service
//   types. So a service type name and an index uniquely identifies an
//   exported offer. In fact, when the register export interface
//   returns a CosTrading::OfferId, it's returning no more than a
//   simple string concatenation of these two values. In addition to
//   all these wonderful things, the TAO_Offer_Database has built-in
//   locking, one reader/writer-style lock for modifying the top-level
//   map and a reader/writer-style for each of the offer
//   maps. Needless to say the locks are acquired when the
//   TAO_Offer_Database performs operations on the structures they
//   guard.
//
//   NOTE: TAO_Offer_Database needs to be parameterized by a
//   READER/WRITER LOCK, a RECURSIVE MUTEX, or a NULL MUTEX, not a
//   simple binary mutex! Mutexes will cause deadlock when you try to
//   contruct an iterator (which acquires a read lock on the map under
//   an existing read lock). Just don't do it, ok?
{
  friend class TAO_Service_Offer_Iterator<LOCK_TYPE>;
public:

  // Traits
  typedef TAO_Service_Offer_Iterator<LOCK_TYPE> offer_iterator;

  TAO_Offer_Database (void);
  // No arg constructor.

  ~TAO_Offer_Database (void);

  CosTrading::OfferId insert_offer (const char* type,
                                    CosTrading::Offer* offer);
  // Add an offer of type <type> and generate a CosTrading::OfferId
  // for it. Returns 0 on failure.

  int remove_offer (const CosTrading::OfferId offer_id,
                    CORBA::Environment& _ACE_TRY_ENV)
    ACE_THROW_SPEC ((CosTrading::IllegalOfferId,
                    CosTrading::UnknownOfferId));

  CosTrading::Offer* lookup_offer (const CosTrading::OfferId offer_id,
                                   CORBA::Environment& ACE_TRY_ENV)
    ACE_THROW_SPEC ((CosTrading::IllegalOfferId,
                    CosTrading::UnknownOfferId));
  // Lookup an offer whose offer_id is <offer_id>, and return
  // it. Otherwise, throw the appropriate exception.

  CosTrading::Offer* lookup_offer (const CosTrading::OfferId offer_id,
                                   char*& type_name,
                                   CORBA::Environment& ACE_TRY_ENV)
    ACE_THROW_SPEC ((CosTrading::IllegalOfferId,
                    CosTrading::UnknownOfferId));
  // Lookup an offer whose OfferId is <offer_id> and return in
  // <type_name> the type name of the object. Type name is just a
  // pointer to a location in offer_id, so DON'T DELETE IT.

  TAO_Offer_Id_Iterator* retrieve_all_offer_ids (void);
  // Return an iterator that will traverse and return all the offer
  // ids in the service type map.

  struct Offer_Map_Entry
  {
    TAO_Offer_Map* offer_map_;
    CORBA::ULong counter_;
    LOCK_TYPE lock_;
  };

  typedef ACE_Hash_Map_Manager_Ex
    <
    TAO_String_Hash_Key,
    Offer_Map_Entry*,
    ACE_Hash<TAO_String_Hash_Key>,
    ACE_Equal_To<TAO_String_Hash_Key>,
    ACE_Null_Mutex
    >
    Offer_Database;

private:

  // The internal id is a pointer here, not only to avoid copying,
  // since we would only copy on insertion, and we only insert once
  // --- with an empty Offer_Map_Entry --- but also since most locks
  // have unimplemented copy constructors.

  CosTrading::Offer* lookup_offer (const char* type,
                                   CORBA::ULong id);
  // Lookup an offer whose type is <type> and id, <id>. Return 0 on
  // failure.

  int remove_offer (const char* type, CORBA::ULong id);
  // Remove an offers whose id is <offer_id>. Returns 0 on success, -1
  // on failure, and throws a CosTrading::IllegalOfferId if it can't
  // parse the CosTrading::OfferId.

  static CosTrading::OfferId generate_offer_id (const char *type_name,
                                                CORBA::ULong id);
  // Take in a service type name for the offer the current value of
  // of the counter and generate an offer id.

  static void parse_offer_id (const CosTrading::OfferId offer_id,
                              char* &service_type,
                              CORBA::ULong& id,
                              CORBA::Environment& ACE_TRY_ENV)
    ACE_THROW_SPEC ((CosTrading::IllegalOfferId));
  // Take in a previously generated offer id and return the type
  // and id that were used to generate the offer id.

  // = Disallow these operations.
  ACE_UNIMPLEMENTED_FUNC (void operator= (const TAO_Offer_Database<LOCK_TYPE> &))
  ACE_UNIMPLEMENTED_FUNC (TAO_Offer_Database (const TAO_Offer_Database<LOCK_TYPE> &))

  LOCK_TYPE db_lock_;

  Offer_Database offer_db_;
  // The protected data structure.
};

template <class LOCK_TYPE>
class TAO_Service_Offer_Iterator
// = TITLE
//   TAO_Service_Offer_Iterator iterates over the set of exported
//   offers for a given type. Handily, it takes care of all the
//   necessary locking, acquiring them in the constructor, and
//   releasing them in the destructor.
{
 public:

  typedef TAO_Offer_Database<LOCK_TYPE> Offer_Database;

  TAO_Service_Offer_Iterator (const char* type,
                              TAO_Offer_Database<LOCK_TYPE>& offer_database);

  ~TAO_Service_Offer_Iterator (void);
  // Release all the locks acquired.

  int has_more_offers (void);
  // Returns 1 if there are more offers, 0 otherwise.

  CosTrading::OfferId get_id (void);
  // Get the id for the current offer.

  CosTrading::Offer* get_offer (void);
  // Returns the next offer in the series.

  void next_offer (void);
  // Advances the iterator 1.

 private:
  // Protected constructor.

  TAO_Offer_Database<LOCK_TYPE>& stm_;
  // Lock the top_level map.

  LOCK_TYPE* lock_;
  // Lock for the internal map.

  TAO_Offer_Map::iterator* offer_iter_;
  // Iterator over the actual offer map.

  const char* type_;
  // The name of the type. Used for constructing offer ids.
};


#if defined (ACE_TEMPLATES_REQUIRE_SOURCE)
#include "Offer_Database.cpp"
#endif  /* ACE_TEMPLATES_REQUIRE_SOURCE */

#include "ace/post.h"
#endif /* TAO_SERVICE_TYPE_MAP_H */
