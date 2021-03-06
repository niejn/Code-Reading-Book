/* -*- C++ -*- */
/**
 *  @file   EC_And_Filter.h
 *
 *  EC_And_Filter.h,v 1.5 2000/10/31 03:11:12 coryan Exp
 *
 *  @author Carlos O'Ryan (coryan@cs.wustl.edu)
 *
 * Based on previous work by Tim Harrison (harrison@cs.wustl.edu) and
 * other members of the DOC group. More details can be found in:
 *
 * http://doc.ece.uci.edu/~coryan/EC/index.html
 */

#ifndef TAO_EC_AND_FILTER_H
#define TAO_EC_AND_FILTER_H
#include "ace/pre.h"

#include "EC_Filter.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
# pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

/**
 * @class TAO_EC_And_Filter
 *
 * @brief The 'logical and' filter.
 *
 * This filter has a set of children (fixed at creation time),
 * only if all the children accept an event it does so too.
 *
 * <H2>Memory Management</H2>
 * It assumes ownership of the children.
 */
class TAO_RTEvent_Export TAO_EC_And_Filter : public TAO_EC_Filter
{
public:
  /// Constructor. It assumes ownership of both the array and the
  /// children.
  TAO_EC_And_Filter (TAO_EC_Filter* children[],
                             size_t n);

  /// Destructor
  virtual ~TAO_EC_And_Filter (void);

  // = The TAO_EC_Filter methods, please check the documentation in
  // TAO_EC_Filter.
  virtual ChildrenIterator begin (void) const;
  virtual ChildrenIterator end (void) const;
  virtual int size (void) const;
  virtual int filter (const RtecEventComm::EventSet& event,
                      TAO_EC_QOS_Info& qos_info,
                      CORBA::Environment& env);
  virtual int filter_nocopy (RtecEventComm::EventSet& event,
                             TAO_EC_QOS_Info& qos_info,
                             CORBA::Environment& env);
  virtual void push (const RtecEventComm::EventSet& event,
                     TAO_EC_QOS_Info& qos_info,
                     CORBA::Environment& env);
  virtual void push_nocopy (RtecEventComm::EventSet& event,
                            TAO_EC_QOS_Info& qos_info,
                            CORBA::Environment& env);
  virtual void clear (void);
  virtual CORBA::ULong max_event_size (void) const;
  virtual int can_match (const RtecEventComm::EventHeader& header) const;
  virtual int add_dependencies (const RtecEventComm::EventHeader& header,
                                const TAO_EC_QOS_Info &qos_info,
                                CORBA::Environment &ACE_TRY_ENV);

  typedef unsigned int Word;

private:
  ACE_UNIMPLEMENTED_FUNC (TAO_EC_And_Filter
                              (const TAO_EC_And_Filter&))
  ACE_UNIMPLEMENTED_FUNC (TAO_EC_And_Filter& operator=
                              (const TAO_EC_And_Filter&))

private:
  /// The children
  TAO_EC_Filter** children_;

  /// The number of children.
  size_t n_;
};

#if defined (__ACE_INLINE__)
#include "EC_And_Filter.i"
#endif /* __ACE_INLINE__ */

#include "ace/post.h"
#endif /* TAO_EC_AND_FILTER_H */
