#ifndef DEBUG_MENU_DEFINE_HPP
#define DEBUG_MENU_DEFINE_HPP

#if !defined( ENABLE_DEBUG_MENU ) && (defined( X_DEBUG ) || ((defined( CONFIG_QA ) || defined( CONFIG_PROFILE )) && (!CONFIG_IS_DEMO) && !defined( OPM_REVIEW_BUILD ) ))
#define ENABLE_DEBUG_MENU
#endif

//==============================================================================
#endif // DEBUG_MENU_DEFINE_HPP
//==============================================================================
