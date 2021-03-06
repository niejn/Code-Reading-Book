// EC_Basic.cpp,v 1.19 2001/03/06 23:48:15 coryan Exp

#include "ace/Get_Opt.h"
#include "ace/Auto_Ptr.h"
#include "ace/Sched_Params.h"

#include "orbsvcs/Event_Utilities.h"
#include "orbsvcs/Event_Service_Constants.h"
#include "orbsvcs/Time_Utilities.h"
#include "orbsvcs/RtecEventChannelAdminC.h"
#include "orbsvcs/Sched/Config_Scheduler.h"
#include "orbsvcs/Runtime_Scheduler.h"
#include "orbsvcs/Event/Event_Channel.h"
#include "orbsvcs/Event/Module_Factory.h"
#include "EC_Basic.h"

#if !defined (__ACE_INLINE__)
#include "EC_Basic.i"
#endif /* __ACE_INLINE__ */

ACE_RCSID(EC_Basic, EC_Basic, "EC_Basic.cpp,v 1.19 2001/03/06 23:48:15 coryan Exp")

// ****************************************************************

int
main (int argc, char *argv [])
{
  ECB_Driver driver;
  return driver.run (argc, argv);
}

// ****************************************************************

ECB_Driver::ECB_Driver (void)
  : pid_filename_ (0)
{
}

int
ECB_Driver::run (int argc, char* argv[])
{
  ACE_DECLARE_NEW_CORBA_ENV;
  ACE_TRY
    {
      this->orb_ =
        CORBA::ORB_init (argc,
                         argv,
                         "",
                         ACE_TRY_ENV);
      ACE_TRY_CHECK;

      CORBA::Object_var poa_object =
        this->orb_->resolve_initial_references ("RootPOA",
                                                ACE_TRY_ENV);
      ACE_TRY_CHECK;

      if (CORBA::is_nil (poa_object.in ()))
        ACE_ERROR_RETURN ((LM_ERROR,
                           " (%P|%t) Unable to initialize the POA.\n"),
                          1);

      PortableServer::POA_var root_poa =
        PortableServer::POA::_narrow (poa_object.in (),
                                      ACE_TRY_ENV);
      ACE_TRY_CHECK;

      PortableServer::POAManager_var poa_manager =
        root_poa->the_POAManager (ACE_TRY_ENV);
      ACE_TRY_CHECK;

      if (this->parse_args (argc, argv))
        return 1;

      ACE_DEBUG ((LM_DEBUG,
                  "EC_Basic: Execution parameters:\n"
                  "  pid file name = <%s>\n",
                  this->pid_filename_ ? this->pid_filename_ : "nil"));

      if (this->pid_filename_ != 0)
        {
          FILE *pid = ACE_OS::fopen (this->pid_filename_,
                                     "w");
          if (pid != 0)
            {
              ACE_OS::fprintf (pid,
                               "%ld\n",
                               ACE_static_cast (long, ACE_OS::getpid ()));
              ACE_OS::fclose (pid);
            }
        }

      ACE_Config_Scheduler scheduler_impl;
      RtecScheduler::Scheduler_var scheduler =
        scheduler_impl._this (ACE_TRY_ENV);
      ACE_TRY_CHECK;

      CORBA::String_var str =
        this->orb_->object_to_string (scheduler.in (),
                                      ACE_TRY_ENV);
      ACE_TRY_CHECK;
      ACE_DEBUG ((LM_DEBUG,
                  "EC_Basic: The (local) scheduler IOR is <%s>\n",
                  str.in ()));

      // Create the EventService implementation, but don't start its
      // internal threads.
      TAO_Reactive_Module_Factory module_factory;
      ACE_EventChannel ec_impl (scheduler.in (),
                                0,
                                ACE_DEFAULT_EVENT_CHANNEL_TYPE,
                                &module_factory);

      // Register Event_Service with the Naming Service.
      RtecEventChannelAdmin::EventChannel_var ec =
        ec_impl._this (ACE_TRY_ENV);
      ACE_TRY_CHECK;

      str = this->orb_->object_to_string (ec.in (),
                                          ACE_TRY_ENV);
      ACE_TRY_CHECK;

      ACE_DEBUG ((LM_DEBUG,
                  "EC_Basic: The (local) EC IOR is <%s>\n",
                  str.in ()));

      poa_manager->activate (ACE_TRY_ENV);
      ACE_TRY_CHECK;

      RtecEventChannelAdmin::EventChannel_var local_ec =
        ec_impl._this (ACE_TRY_ENV);
      ACE_TRY_CHECK;

      ec_impl.activate ();

      ACE_DEBUG ((LM_DEBUG,
                  "EC_Basic: local EC objref ready\n"));

      ACE_DEBUG ((LM_DEBUG,
                  "EC_Basic: start supplier_id_test\n"));

      ECB_SupplierID_Test supplier_id_test;
      supplier_id_test.run (this->orb_.in (),
                            local_ec.in (),
                            scheduler.in (),
                            ACE_TRY_ENV);
      ACE_TRY_CHECK;

      if (supplier_id_test.dump_results () != 0)
        ACE_ERROR_RETURN ((LM_ERROR,
                           "EC_Basic: supplier_id test failed\n"),
                          -1);
      ACE_DEBUG ((LM_DEBUG,
                  "EC_Basic: end supplier_id_test\n"));

      ACE_DEBUG ((LM_DEBUG,
                  "EC_Basic: start correlation_test\n"));

      ECB_Correlation_Test correlation_test;
      correlation_test.run (this->orb_.in (),
                            local_ec.in (),
                            scheduler.in (),
                            ACE_TRY_ENV);
      ACE_TRY_CHECK;

      if (correlation_test.dump_results () != 0)
        ACE_ERROR_RETURN ((LM_ERROR,
                           "EC_Basic: correlation test failed\n"),
                          -1);
      ACE_DEBUG ((LM_DEBUG,
                  "EC_Basic: end correlation_test\n"));

      ACE_DEBUG ((LM_DEBUG,
                  "EC_Basic: shutdown the EC\n"));
      ec_impl.shutdown ();
    }
  ACE_CATCH (CORBA::SystemException, sys_ex)
    {
      ACE_PRINT_EXCEPTION (sys_ex, "SYS_EX");
    }
  ACE_CATCHANY
    {
      ACE_PRINT_EXCEPTION (ACE_ANY_EXCEPTION, "NON SYS EX");
    }
  ACE_ENDTRY;
  return 0;
}

// ****************************************************************

int
ECB_Driver::parse_args (int argc, char *argv [])
{
  ACE_Get_Opt get_opt (argc, argv, "p:");
  int opt;

  while ((opt = get_opt ()) != EOF)
    {
      switch (opt)
        {
        case 'p':
          this->pid_filename_ = get_opt.optarg;
          break;
        case '?':
        default:
          ACE_DEBUG ((LM_DEBUG,
                      "Usage: %s "
                      "[ORB options] "
                      "-p <pid file name> "
                      "\n",
                      argv[0]));
          return -1;
        }
    }

  return 0;
}

// ****************************************************************

ECB_Test::~ECB_Test (void)
{
}

// ****************************************************************

ECB_Consumer::ECB_Consumer (ECB_Test *test,
                            int consumer_id)
  :  test_ (test),
     consumer_id_ (consumer_id)
{
}

void
ECB_Consumer::open (const char* name,
                    RtecEventChannelAdmin::EventChannel_ptr ec,
                    RtecScheduler::Scheduler_ptr scheduler,
                    CORBA::Environment& ACE_TRY_ENV)
{
  this->rt_info_ =
    scheduler->create (name, ACE_TRY_ENV);
  ACE_CHECK;

  // The worst case execution time is far less than 2
  // milliseconds, but that is a safe estimate....
  ACE_Time_Value tv (0, 2000);
  TimeBase::TimeT time;
  ORBSVCS_Time::Time_Value_to_TimeT (time, tv);
  scheduler->set (this->rt_info_,
                  RtecScheduler::VERY_HIGH_CRITICALITY,
                  time, time, time,
                  0,
                  RtecScheduler::VERY_LOW_IMPORTANCE,
                  time,
                  0,
                  RtecScheduler::OPERATION,
                  ACE_TRY_ENV);
  ACE_CHECK;

  // = Connect as a consumer.
  this->consumer_admin_ = ec->for_consumers (ACE_TRY_ENV);
  ACE_CHECK;
}

void
ECB_Consumer::connect (const RtecEventChannelAdmin::ConsumerQOS& qos,
                       CORBA::Environment& ACE_TRY_ENV)
{
  if (CORBA::is_nil (this->consumer_admin_.in ()))
    return;

  RtecEventComm::PushConsumer_var objref = this->_this (ACE_TRY_ENV);
  ACE_CHECK;

  this->supplier_proxy_ =
    this->consumer_admin_->obtain_push_supplier (ACE_TRY_ENV);
  ACE_CHECK;

  this->supplier_proxy_->connect_push_consumer (objref.in (),
                                                qos,
                                                ACE_TRY_ENV);
  ACE_CHECK;
}

void
ECB_Consumer::disconnect (CORBA::Environment& ACE_TRY_ENV)
{
  if (CORBA::is_nil (this->supplier_proxy_.in ())
      || CORBA::is_nil (this->consumer_admin_.in ()))
    return;

  RtecEventChannelAdmin::ProxyPushSupplier_var tmp =
    this->supplier_proxy_._retn ();
  tmp->disconnect_push_supplier (ACE_TRY_ENV);
  ACE_CHECK;
}

void
ECB_Consumer::close (CORBA::Environment &ACE_TRY_ENV)
{
  this->disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer_admin_ =
    RtecEventChannelAdmin::ConsumerAdmin::_nil ();
}

void
ECB_Consumer::push (const RtecEventComm::EventSet& events,
                    CORBA::Environment &ACE_TRY_ENV)
    ACE_THROW_SPEC ((CORBA::SystemException))
{
  this->test_->push (this->consumer_id_,
                     events,
                     ACE_TRY_ENV);
}

void
ECB_Consumer::disconnect_push_consumer (CORBA::Environment &)
    ACE_THROW_SPEC ((CORBA::SystemException))
{
}

// ****************************************************************

ECB_Supplier::ECB_Supplier (ECB_Test *test,
                            int supplier_id)
  :  test_ (test),
     supplier_id_ (supplier_id)
{
}

void
ECB_Supplier::open (const char* name,
                    RtecEventChannelAdmin::EventChannel_ptr ec,
                    RtecScheduler::Scheduler_ptr scheduler,
                    CORBA::Environment &ACE_TRY_ENV)
{
  this->rt_info_ =
    scheduler->create (name, ACE_TRY_ENV);
  ACE_CHECK;

  // The execution times are set to reasonable values, but actually
  // they are changed on the real execution, i.e. we lie to the
  // scheduler to obtain right priorities; but we don't care if the
  // set is schedulable.
  ACE_Time_Value tv (0, 2000);
  TimeBase::TimeT time;
  ORBSVCS_Time::Time_Value_to_TimeT (time, tv);

  scheduler->set (this->rt_info_,
                  RtecScheduler::VERY_HIGH_CRITICALITY,
                  time, time, time,
                  0,
                  RtecScheduler::VERY_LOW_IMPORTANCE,
                  time,
                  1,
                  RtecScheduler::OPERATION,
                  ACE_TRY_ENV);
  ACE_CHECK;

  // = Connect as a consumer.
  this->supplier_admin_ = ec->for_suppliers (ACE_TRY_ENV);
  ACE_CHECK;
}

void
ECB_Supplier::connect (const RtecEventChannelAdmin::SupplierQOS& qos,
                       CORBA::Environment& ACE_TRY_ENV)
{
  if (CORBA::is_nil (this->supplier_admin_.in ()))
    return;

  this->consumer_proxy_ =
    this->supplier_admin_->obtain_push_consumer (ACE_TRY_ENV);
  ACE_CHECK;

  RtecEventComm::PushSupplier_var objref = this->_this (ACE_TRY_ENV);
  ACE_CHECK;

  this->consumer_proxy_->connect_push_supplier (objref.in (),
                                                qos,
                                                ACE_TRY_ENV);
  ACE_CHECK;
}

void
ECB_Supplier::disconnect (CORBA::Environment& ACE_TRY_ENV)
{
  if (CORBA::is_nil (this->consumer_proxy_.in ()))
    return;

  RtecEventChannelAdmin::ProxyPushConsumer_var proxy =
    this->consumer_proxy_._retn ();
  proxy->disconnect_push_consumer (ACE_TRY_ENV);
}

void
ECB_Supplier::close (CORBA::Environment &ACE_TRY_ENV)
{
  if (CORBA::is_nil (this->supplier_admin_.in ()))
    return;

  this->supplier_admin_ =
    RtecEventChannelAdmin::SupplierAdmin::_nil ();

  this->disconnect (ACE_TRY_ENV);
}

void
ECB_Supplier::send_event (RtecEventComm::EventSet& events,
                          CORBA::Environment& ACE_TRY_ENV)
{
  // RtecEventComm::EventSet copy = events;
  this->consumer_proxy_->push (events, ACE_TRY_ENV);
}

void
ECB_Supplier::disconnect_push_supplier (CORBA::Environment& /* ACE_TRY_ENV */)
    ACE_THROW_SPEC ((CORBA::SystemException))
{
  // this->supplier_proxy_->disconnect_push_supplier (ACE_TRY_ENV);
}

// ****************************************************************

ECB_SupplierID_Test::ECB_SupplierID_Test (void)
  :  consumer0_ (this, 0),
     consumer1_ (this, 1),
     supplier0_ (this, 0),
     supplier1_ (this, 1)
{
}

void
ECB_SupplierID_Test::run (CORBA::ORB_ptr orb,
                          RtecEventChannelAdmin::EventChannel_ptr ec,
                          RtecScheduler::Scheduler_ptr scheduler,
                          CORBA::Environment& ACE_TRY_ENV)
{
  ACE_UNUSED_ARG (orb);

  int i;

  for (i = 0; i <= ECB_SupplierID_Test::PHASE_END; ++i)
    {
      this->event_count_[i] = 0;
      this->error_count_[i] = 0;
    }

  // Startup
  this->consumer0_.open ("SupplierID/consumer0",
                         ec,
                         scheduler,
                         ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer1_.open ("SupplierID/consumer1",
                         ec,
                         scheduler,
                         ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier0_.open ("SupplierID/supplier0",
                         ec,
                         scheduler,
                         ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.open ("SupplierID/supplier1",
                         ec,
                         scheduler,
                         ACE_TRY_ENV);
  ACE_CHECK;

  // Precompute the QoS for the consumers and suppliers.
  ACE_ConsumerQOS_Factory consumer0_qos;
  consumer0_qos.start_disjunction_group ();
  consumer0_qos.insert_source (ECB_SupplierID_Test::SUPPLIER_ID,
                               this->consumer0_.rt_info ());

  ACE_ConsumerQOS_Factory consumer1_qos;
  consumer1_qos.start_disjunction_group ();
  consumer1_qos.insert_source (ECB_SupplierID_Test::SUPPLIER_ID,
                               this->consumer1_.rt_info ());

  ACE_SupplierQOS_Factory supplier0_qos;
  supplier0_qos.insert (ECB_SupplierID_Test::SUPPLIER_ID,
                        ACE_ES_EVENT_UNDEFINED + 1,
                        this->supplier0_.rt_info (),
                        1);

  ACE_SupplierQOS_Factory supplier1_qos;
  supplier1_qos.insert (ECB_SupplierID_Test::SUPPLIER_ID,
                        ACE_ES_EVENT_UNDEFINED + 1,
                        this->supplier1_.rt_info (),
                        1);

  // Precompute the event set
  RtecEventComm::EventSet events (1);
  events.length (1);

  RtecEventComm::Event& e = events[0];
  e.header.source = ECB_SupplierID_Test::SUPPLIER_ID;
  e.header.ttl = 1;
  e.header.type = ACE_ES_EVENT_UNDEFINED + 1;

  ACE_hrtime_t t = ACE_OS::gethrtime ();
  ORBSVCS_Time::hrtime_to_TimeT (e.header.creation_time, t);

  // Start the real test.

  // PHASE 0, test filtering by supplier ID in the presence of
  // multiple suppliers with the same ID...
  this->phase_ = ECB_SupplierID_Test::PHASE_0;

  this->consumer0_.connect (consumer0_qos.get_ConsumerQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer1_.connect (consumer1_qos.get_ConsumerQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;

  this->supplier0_.connect (supplier0_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.connect (supplier1_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_SupplierID_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE 1, test disconnection of a single supplier.
  this->phase_ = ECB_SupplierID_Test::PHASE_1;
  this->supplier1_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_SupplierID_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE 2, test reconnection of the supplier.
  this->phase_ = ECB_SupplierID_Test::PHASE_2;
  this->supplier1_.connect (supplier1_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_SupplierID_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE 3, test disconnect of one consumer
  this->phase_ = ECB_SupplierID_Test::PHASE_3;
  this->consumer1_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_SupplierID_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE 4, test reconnection of one consumer
  this->phase_ = ECB_SupplierID_Test::PHASE_4;
  this->consumer1_.connect (consumer1_qos.get_ConsumerQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_SupplierID_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE 5, test disconnection of two consumers.
  this->phase_ = ECB_SupplierID_Test::PHASE_5;
  this->consumer0_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer1_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_SupplierID_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE 6, test reconnection of two consumers.
  this->phase_ = ECB_SupplierID_Test::PHASE_6;
  this->consumer0_.connect (consumer0_qos.get_ConsumerQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer1_.connect (consumer1_qos.get_ConsumerQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_SupplierID_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE 7, test disconnect/reconnect of both suppliers.
  this->phase_ = ECB_SupplierID_Test::PHASE_7;
  this->supplier0_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier0_.connect (supplier0_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.connect (supplier1_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_SupplierID_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (events, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE END, any events received after this are errors.
  this->phase_ = ECB_SupplierID_Test::PHASE_END;

  // Finish
  this->supplier1_.close (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier0_.close (ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer1_.close (ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer0_.close (ACE_TRY_ENV);
  ACE_CHECK;
}

int
ECB_SupplierID_Test::dump_results (void)
{
  static CORBA::ULong expected_count[PHASE_END]={
    4 * ECB_SupplierID_Test::EVENTS_SENT,
    2 * ECB_SupplierID_Test::EVENTS_SENT,
    4 * ECB_SupplierID_Test::EVENTS_SENT,
    2 * ECB_SupplierID_Test::EVENTS_SENT,
    4 * ECB_SupplierID_Test::EVENTS_SENT,
    0,
    4 * ECB_SupplierID_Test::EVENTS_SENT,
    4 * ECB_SupplierID_Test::EVENTS_SENT
  };

  int result = 0;

  for (int i = 0;
       i < ECB_SupplierID_Test::PHASE_END;
       ++i)
    {
      if (this->error_count_[i] != 0)
        {
          ACE_ERROR ((LM_ERROR,
                      "SupplierID_Test: Error count for phase %d "
                      "is not zero\n",
                      i));
          result = -1;
        }
      if (this->event_count_[i] != expected_count[i])
        {
          ACE_ERROR ((LM_ERROR,
                      "SupplierID_Test: Mismatched event count in phase %d, "
                      "expected %d, count is %d\n",
                      i,
                      expected_count[i],
                      this->event_count_[i]));
          result = -1;
        }
    }
  if (this->error_count_[ECB_SupplierID_Test::PHASE_END] != 0)
    {
      ACE_ERROR ((LM_ERROR,
                  "SupplierID_Test: Events received after final phase\n"));
      result = -1;
    }
  if (result == 0)
    ACE_DEBUG ((LM_DEBUG,
                "SupplierID_Test: All phases successful\n"));

  return result;
}

void
ECB_SupplierID_Test::push (int consumer_id,
                           const RtecEventComm::EventSet &,
                           CORBA::Environment &)
{
  switch (this->phase_)
    {
    case ECB_SupplierID_Test::PHASE_END:
    default:
      this->error_count_[ECB_SupplierID_Test::PHASE_END]++;
      break;

    case ECB_SupplierID_Test::PHASE_0:
      this->event_count_[ECB_SupplierID_Test::PHASE_0]++;
      break;

    case ECB_SupplierID_Test::PHASE_1:
      this->event_count_[ECB_SupplierID_Test::PHASE_1]++;
      break;

    case ECB_SupplierID_Test::PHASE_2:
      this->event_count_[ECB_SupplierID_Test::PHASE_2]++;
      break;

    case ECB_SupplierID_Test::PHASE_3:
      if (consumer_id == 0)
        this->event_count_[ECB_SupplierID_Test::PHASE_3]++;
      else
        this->error_count_[ECB_SupplierID_Test::PHASE_3]++;
      break;

    case ECB_SupplierID_Test::PHASE_4:
      this->event_count_[ECB_SupplierID_Test::PHASE_4]++;
      break;

    case ECB_SupplierID_Test::PHASE_5:
      this->error_count_[ECB_SupplierID_Test::PHASE_5]++;
      break;

    case ECB_SupplierID_Test::PHASE_6:
      this->event_count_[ECB_SupplierID_Test::PHASE_6]++;
      break;

    case ECB_SupplierID_Test::PHASE_7:
      this->event_count_[ECB_SupplierID_Test::PHASE_7]++;
      break;
    }
}

// ****************************************************************

ECB_Correlation_Test::ECB_Correlation_Test (void)
  :  consumer_ (this, 0),
     supplier0_ (this, 0),
     supplier1_ (this, 1)
{
}

void
ECB_Correlation_Test::run (CORBA::ORB_ptr orb,
                           RtecEventChannelAdmin::EventChannel_ptr ec,
                           RtecScheduler::Scheduler_ptr scheduler,
                           CORBA::Environment& ACE_TRY_ENV)
{
  ACE_UNUSED_ARG (orb);

  int i;

  for (i = 0; i <= ECB_Correlation_Test::PHASE_END; ++i)
    {
      this->event_count_[i] = 0;
      this->error_count_[i] = 0;
    }

  // Startup
  this->consumer_.open ("Correlation/consumer",
                        ec,
                        scheduler,
                        ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier0_.open ("Correlation/supplier0",
                         ec,
                         scheduler,
                         ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.open ("Correlation/supplier1",
                         ec,
                         scheduler,
                         ACE_TRY_ENV);
  ACE_CHECK;

  // Precompute the QoS for the consumers and suppliers.
  ACE_ConsumerQOS_Factory consumer_qos;
  consumer_qos.start_conjunction_group ();
  consumer_qos.insert_type (ECB_Correlation_Test::EVENT_A,
                            this->consumer_.rt_info ());
  consumer_qos.insert_type (ECB_Correlation_Test::EVENT_B,
                            this->consumer_.rt_info ());

  ACE_SupplierQOS_Factory supplier0_qos;
  supplier0_qos.insert (ECB_Correlation_Test::SUPPLIER_ID_0,
                        ECB_Correlation_Test::EVENT_A,
                        this->supplier0_.rt_info (),
                        1);
  supplier0_qos.insert (ECB_Correlation_Test::SUPPLIER_ID_0,
                        ECB_Correlation_Test::EVENT_B,
                        this->supplier0_.rt_info (),
                        1);

  ACE_SupplierQOS_Factory supplier1_qos;
  supplier1_qos.insert (ECB_Correlation_Test::SUPPLIER_ID_1,
                        ECB_Correlation_Test::EVENT_A,
                        this->supplier1_.rt_info (),
                        1);
  supplier1_qos.insert (ECB_Correlation_Test::SUPPLIER_ID_1,
                        ECB_Correlation_Test::EVENT_B,
                        this->supplier1_.rt_info (),
                        1);

  // Precompute the events
  RtecEventComm::EventSet event_a (1);
  event_a.length (1);
  {
    RtecEventComm::Event& e = event_a[0];
    e.header.source = ECB_Correlation_Test::SUPPLIER_ID_0;
    e.header.ttl = 1;
    e.header.type = ECB_Correlation_Test::EVENT_A;

    ACE_hrtime_t t = ACE_OS::gethrtime ();
    ORBSVCS_Time::hrtime_to_TimeT (e.header.creation_time, t);
  }

  RtecEventComm::EventSet event_b (1);
  event_b.length (1);
  {
    RtecEventComm::Event& e = event_b[0];
    e.header.source = ECB_Correlation_Test::SUPPLIER_ID_0;
    e.header.ttl = 1;
    e.header.type = ECB_Correlation_Test::EVENT_B;

    ACE_hrtime_t t = ACE_OS::gethrtime ();
    ORBSVCS_Time::hrtime_to_TimeT (e.header.creation_time, t);
  }

  RtecEventComm::EventSet event_ab (2);
  event_ab.length (2);
  {
    RtecEventComm::Event& e = event_ab[0];
    e.header.source = ECB_Correlation_Test::SUPPLIER_ID_0;
    e.header.ttl = 1;
    e.header.type = ECB_Correlation_Test::EVENT_A;

    ACE_hrtime_t t = ACE_OS::gethrtime ();
    ORBSVCS_Time::hrtime_to_TimeT (e.header.creation_time, t);
  }
  {
    RtecEventComm::Event& e = event_ab[1];
    e.header.source = ECB_Correlation_Test::SUPPLIER_ID_0;
    e.header.ttl = 1;
    e.header.type = ECB_Correlation_Test::EVENT_B;

    ACE_hrtime_t t = ACE_OS::gethrtime ();
    ORBSVCS_Time::hrtime_to_TimeT (e.header.creation_time, t);
  }

  // Start the real test.

  // PHASE 0
  this->phase_ = ECB_Correlation_Test::PHASE_0;

  this->consumer_.connect (consumer_qos.get_ConsumerQOS (),
                           ACE_TRY_ENV);
  ACE_CHECK;

  this->supplier0_.connect (supplier0_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.connect (supplier1_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_Correlation_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (event_a, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (event_b, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE 1, test disconnection of a single supplier.
  this->phase_ = ECB_Correlation_Test::PHASE_1;
  this->consumer_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier0_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer_.connect (consumer_qos.get_ConsumerQOS (),
                           ACE_TRY_ENV);
  ACE_CHECK;

  this->supplier0_.connect (supplier0_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.connect (supplier1_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_Correlation_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (event_ab, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (event_ab, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE 2, test reconnection of the supplier.
  this->phase_ = ECB_Correlation_Test::PHASE_2;
  this->consumer_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier0_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer_.connect (consumer_qos.get_ConsumerQOS (),
                           ACE_TRY_ENV);
  ACE_CHECK;

  this->supplier0_.connect (supplier0_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.connect (supplier1_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_Correlation_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (event_a, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (event_b, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier0_.send_event (event_b, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (event_a, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE 3
  this->phase_ = ECB_Correlation_Test::PHASE_3;
  this->consumer_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier0_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer_.connect (consumer_qos.get_ConsumerQOS (),
                           ACE_TRY_ENV);
  ACE_CHECK;

  this->supplier0_.connect (supplier0_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.connect (supplier1_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_Correlation_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (event_a, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (event_ab, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE 4
  this->phase_ = ECB_Correlation_Test::PHASE_4;
  this->consumer_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier0_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer_.connect (consumer_qos.get_ConsumerQOS (),
                           ACE_TRY_ENV);
  ACE_CHECK;

  this->supplier0_.connect (supplier0_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.connect (supplier1_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_Correlation_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (event_a, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (event_a, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (event_b, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE 5
  this->phase_ = ECB_Correlation_Test::PHASE_5;
  this->consumer_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier0_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.disconnect (ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer_.connect (consumer_qos.get_ConsumerQOS (),
                           ACE_TRY_ENV);
  ACE_CHECK;

  this->supplier0_.connect (supplier0_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier1_.connect (supplier1_qos.get_SupplierQOS (),
                            ACE_TRY_ENV);
  ACE_CHECK;

  for (i = 0; i < ECB_Correlation_Test::EVENTS_SENT; ++i)
    {
      this->supplier0_.send_event (event_a, ACE_TRY_ENV);
      ACE_CHECK;
      this->supplier1_.send_event (event_a, ACE_TRY_ENV);
      ACE_CHECK;
    }

  // PHASE END, any events received after this are errors.
  this->phase_ = ECB_Correlation_Test::PHASE_END;

  // Finish
  this->supplier1_.close (ACE_TRY_ENV);
  ACE_CHECK;
  this->supplier0_.close (ACE_TRY_ENV);
  ACE_CHECK;
  this->consumer_.close (ACE_TRY_ENV);
  ACE_CHECK;
}

int
ECB_Correlation_Test::dump_results (void)
{
  static CORBA::ULong expected_count[PHASE_END] =
  {
    1 * ECB_Correlation_Test::EVENTS_SENT,
    2 * ECB_Correlation_Test::EVENTS_SENT,
    2 * ECB_Correlation_Test::EVENTS_SENT,
    1 * ECB_Correlation_Test::EVENTS_SENT,
    1 * ECB_Correlation_Test::EVENTS_SENT,
    0
  };

  int result = 0;

  for (int i = 0;
       i < ECB_Correlation_Test::PHASE_END;
       ++i)
    {
      if (this->error_count_[i] != 0)
        {
          ACE_ERROR ((LM_ERROR,
                      "Correlation_Test: Error count for phase %d "
                      "is not zero\n",
                      i));
          result = -1;
        }
      if (this->event_count_[i] != expected_count[i])
        {
          ACE_ERROR ((LM_ERROR,
                      "Correlation_Test: Mismatched event count in phase %d, "
                      "expected %d, count is %d\n",
                      i,
                      expected_count[i],
                      this->event_count_[i]));
          result = -1;
        }
    }
  if (this->error_count_[ECB_Correlation_Test::PHASE_END] != 0)
    {
      ACE_ERROR ((LM_ERROR,
                  "Correlation_Test: Events received after final phase\n"));
      result = -1;
    }
  if (result == 0)
    ACE_DEBUG ((LM_DEBUG,
                "Correlation_Test: All phases successful\n"));

  return result;
}

void
ECB_Correlation_Test::push (int,
                            const RtecEventComm::EventSet& events,
                            CORBA::Environment &)
{
  if (events.length () != 2)
    {
      ACE_ERROR ((LM_ERROR,
                  "Correlation_Test::push - "
                  "event length (%d) in phase %d\n",
                  events.length (), this->phase_));
      this->error_count_[this->phase_]++;
      return;
    }

  // If the types do not match we have an error.
  if (!((events[0].header.type == ECB_Correlation_Test::EVENT_A
         && events[1].header.type == ECB_Correlation_Test::EVENT_B)
        || (events[0].header.type == ECB_Correlation_Test::EVENT_B
            && events[1].header.type == ECB_Correlation_Test::EVENT_A)))
    {
      ACE_ERROR ((LM_ERROR,
                  "Correlation_Test::push - event type\n"));
      this->error_count_[this->phase_]++;
      return;
    }

  switch (this->phase_)
    {
    case ECB_Correlation_Test::PHASE_END:
    default:
      this->error_count_[ECB_Correlation_Test::PHASE_END]++;
      break;

    case ECB_Correlation_Test::PHASE_0:
      this->event_count_[ECB_Correlation_Test::PHASE_0]++;
      break;

    case ECB_Correlation_Test::PHASE_1:
      this->event_count_[ECB_Correlation_Test::PHASE_1]++;
      break;

    case ECB_Correlation_Test::PHASE_2:
      this->event_count_[ECB_Correlation_Test::PHASE_2]++;
      break;

    case ECB_Correlation_Test::PHASE_3:
      this->event_count_[ECB_Correlation_Test::PHASE_3]++;
      break;

    case ECB_Correlation_Test::PHASE_4:
      this->event_count_[ECB_Correlation_Test::PHASE_4]++;
      break;

    case ECB_Correlation_Test::PHASE_5:
      this->error_count_[ECB_Correlation_Test::PHASE_5]++;
      break;
    }
}

// ****************************************************************

#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)
#elif defined(ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)
#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */
