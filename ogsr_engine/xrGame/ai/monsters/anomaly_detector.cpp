#include "stdafx.h"
#include "anomaly_detector.h"
#include "../../CustomMonster.h"
#include "../../restricted_object.h"
#include "../../customzone.h"
#include "../../level.h"
#include "../../space_restriction_manager.h"
#include "../../movement_manager.h"
#include "ai_monster_utils.h"

CAnomalyDetector::CAnomalyDetector(CCustomMonster *monster) : m_object(monster)
{
}

CAnomalyDetector::~CAnomalyDetector()
{
}

void CAnomalyDetector::load(LPCSTR section)
{
	m_radius		= READ_IF_EXISTS( pSettings, r_float, section, "Anomaly_Detect_Radius", 15.f );
	m_time_to_rememeber	= READ_IF_EXISTS( pSettings, r_u32, section, "Anomaly_Detect_Time_Remember", 30000 );
	m_detect_probability	= READ_IF_EXISTS( pSettings, r_float, section, "Anomaly_Detect_Probability", 1.f );
}

void CAnomalyDetector::reinit()
{
	m_storage.clear();

	m_active = false;
	m_forced = false;
}


void CAnomalyDetector::update_schedule()
{
	if (m_active)
		m_object->feel_touch_update(m_object->Position(), m_radius);

	if (m_storage.empty()) 
		return;

	xr_vector<u16>			temp_out_restrictors;
	xr_vector<u16>			temp_in_restrictors;
	
	temp_in_restrictors.reserve(m_storage.size());
	
	// add new restrictions
	for (ANOMALY_INFO_VEC_IT it = m_storage.begin(); it != m_storage.end(); it++) {
		if (it->time_registered == 0) {
			temp_in_restrictors.push_back(it->id);
			it->time_registered = time();
		}
	}
	m_object->movement().restrictions().add_restrictions(temp_out_restrictors,temp_in_restrictors);

	// remove old restrictions
	temp_in_restrictors.clear();
	for ( ANOMALY_INFO_VEC_IT it = m_storage.begin(); it != m_storage.end(); it++ ) {
	  if ( it->time_registered + m_time_to_rememeber < time() && !it->ignored )
	    temp_in_restrictors.push_back( it->id );
	}
	m_object->movement().restrictions().remove_restrictions(temp_out_restrictors,temp_in_restrictors);

	
	// remove from storage
	m_storage.erase		(
		std::remove_if(
			m_storage.begin(),
			m_storage.end(),
			remove_predicate(m_time_to_rememeber)
		),
		m_storage.end()
	);
}

void CAnomalyDetector::on_contact(CObject *obj)
{
	if (!m_active) return;
	
	CCustomZone	*custom_zone = smart_cast<CCustomZone*>(obj);
	if (!custom_zone) return;
	
	// if its NOT A restrictor - skip
	if (custom_zone->restrictor_type() == RestrictionSpace::eRestrictorTypeNone) return;

	auto it = std::find_if(
	  m_storage.begin(), m_storage.end(), [ custom_zone ]( const auto it ) {
	    return it.id == custom_zone->ID();
	  }
	);
	if ( it != m_storage.end() ) {
	  it->time_registered = time();
	  return;
	}
	
	if (Level().space_restriction_manager().restriction_presented(
		m_object->movement().restrictions().in_restrictions(),custom_zone->cName())) return;


	SAnomalyInfo		info;
	info.id			= obj->ID();
	if ( Random.randF() >= m_detect_probability && !fsimilar( m_detect_probability, 1.f ) ) {
	  info.ignored         = true;
	  info.time_registered = time();
	}
	else {
	  info.ignored         = false;
	  info.time_registered = 0;
	}
	m_storage.push_back	(info);
}


void CAnomalyDetector::activate( bool force ) {
  if ( m_forced && !force ) return;
  m_forced = force;
  m_active = true;
}


void CAnomalyDetector::deactivate( bool force ) {
  if ( m_forced && !force ) return;
  m_forced = force;
  m_active = false;
}


void CAnomalyDetector::remove_all_restrictions() {
  xr_vector<u16> temp_out_restrictors;
  xr_vector<u16> temp_in_restrictors;

  temp_in_restrictors.reserve( m_storage.size() );
  for ( ANOMALY_INFO_VEC_IT it = m_storage.begin(); it != m_storage.end(); it++ )
    temp_in_restrictors.push_back( it->id );
  m_object->movement().restrictions()
    .remove_restrictions( temp_out_restrictors, temp_in_restrictors );
  m_storage.clear();
}


void CAnomalyDetector::remove_restriction( u16 id ) {
  auto it = std::find_if(
    m_storage.begin(), m_storage.end(), [ id ]( const auto it ) {
      return it.id == id;
    }
  );

  if ( it != m_storage.end() ) {
    xr_vector<u16> temp_out_restrictors;
    xr_vector<u16> temp_in_restrictors;
    temp_in_restrictors.push_back( it->id );
    m_object->movement().restrictions()
      .remove_restrictions( temp_out_restrictors, temp_in_restrictors );
    m_storage.erase( it );
  }
}
