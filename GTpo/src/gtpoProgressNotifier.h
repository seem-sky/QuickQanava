/*
    The MIT License (MIT)

    Copyright (c) 2016 Benoit AUTHEMAN

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

//-----------------------------------------------------------------------------
// This file is a part of the GTpo software library.
//
// \file	gtpoProgressNotifier.h
// \author	benoit@qanava.org
// \date	2016 01 31
//-----------------------------------------------------------------------------

#ifndef gtpoProgressNotifier_h
#define gtpoProgressNotifier_h

// GTpo headers
#include "./gtpoUtils.h"
#include "./gtpoGraph.h"

// STD headers
#include <fstream>
#include <atomic>

namespace gtpo { // ::gtpo

class ProgressNotifier;

/*! \brief Standard interface used to monitor progress long operations, either synchronous or asynchronous, such as in/out serialization.
 *
 * GTpo support two related kind of progress concepts:
 * \li overall progress: global task completion between 0.0 and 1.0 with setProgress() and getProgress() methods
 * \li phase progress: a long operation could be split in different phases (for example node serialization phase, then
 * edge serialization and so on), where each phase has a progress between 0.0 and 1.0.
 *
 * Here is a summary of the interaction between a long \c task and a progress \c notifier:
 * \li Progress \c notifier is passed as an argument of the long \c task method or functor.
 * \li First, the task initialize the number of phases that are usually required to complete the task with a call to setPhaseCount().
 * \li When the \c task change phase, the nextPhase() method is called.
 * \li The \c task then set the current phase progress with calls to setPhaseProgress(), the overall progress
 * is automatically updated by this notifyer and could be accessed with getProgress().
 * \li When the task ends, it call endProgress().
 *
 * Example code with a notifier and a sub notifier for sub tasks:
 * \code
 *  void    Consumer::consume( gtpo::IProgressNotifier* notifier )
 *   {
 *       if ( notifier == nullptr )
 *           return;
 *       notifier->reset();
 *       notifier->setPhaseCount( 3 );
 *       gtpo::IProgressNotifier* subNotifier = notifier->createSubProgress();
 *       subNotifier->setPhaseCount( 2 );
 *       notifier->beginProgress();
 *
 *       subNotifier->beginProgress();
 *       subNotifier->beginPhase( "Sub progress - phase1" );
 *       consumePhase( subNotifier );
 *       subNotifier->beginPhase( "Sub Progress - phase2" );
 *       consumePhase( subNotifier );
 *       subNotifier->endProgress();
 *
 *       notifier->beginPhase( "Super progress - phase1");
 *       consumePhase( notifier );
 *       notifier->beginPhase( "Super progress - phase2" );
 *       consumePhase( notifier );
 *       notifier->beginPhase( "Super progress - phase3" );
 *       consumePhase( notifier );
 *
 *       notifier->endProgress();
 *  }
 * \endcode
 *
 * Update of overall or phase progress is protected against values outside of the [0.;1.0] range.
 *
 * IProgressNotifier is the default progress notifier, and it don't has state nor notify anything, any decent compiler
 * should just generate no overhead at runtime when parametrized with such class.
 *
 */
class IProgressNotifier
{
public:
    //! Construct a default notifiyer with 0.0 progress and 0 \c phaseCount.
    IProgressNotifier( IProgressNotifier* superProgress = nullptr ) :
        _superProgress( superProgress ) { }
    virtual ~IProgressNotifier() { _notifiers.clear(); }

protected:
    IProgressNotifier*  getSuperProgress( ) { return _superProgress; }
private:
    IProgressNotifier*  _superProgress = nullptr;

public:
    //! Called by a sub progress to force progress update of a super progress.
    virtual void    update( ) {
        if ( getSuperProgress() != nullptr )    // Update any eventual super progress
            getSuperProgress()->update();
    }

    //! Reset the progress notifier (destroy all sub progress).
    virtual void    reset() {
        _notifiers.clear();
    }

    using   IProgressNotifierPtr = std::unique_ptr< IProgressNotifier >;

    /*! Return a new sub progress notifier (this progress keep ownership for the sub progress).
     *
     * Creating a sub progress will introduce a new level in progress reporting. For example, if you are
     * extending GTpo, you might have custom serialization to do with its own progress reporting. In your
     * serialization method:
     * \li Create a progress object.
     * \li Add a new sub progress.
     * \li Call base GTpo serialization methods with the subprogress.
     * \li Then, serialize your data using the root progress notifier.
     *
     * \code
     *    gtpo::ProgressNotifier progress;
     *    gtpo::IProgressNotifier* progress2 = progress.createSubProgress();
     *
     *    // Then report the serialization of your own data in progress
     * \endcode
     *
     */
    virtual IProgressNotifier*  createSubProgress() {
        IProgressNotifier* subProgress = new IProgressNotifier( this );
        IProgressNotifierPtr progressPtr( subProgress );
        _notifiers.emplace_back( std::move( progressPtr ) );
        return subProgress;
    }

protected:
    std::vector< IProgressNotifierPtr > _notifiers;

public:
    //! Return the current overall progress, taking into account configured \c phaseCount and current phase progress.
    virtual double  getProgress() const { return 0.; }

    //! Called for user notification purposes when notifier change (default implementation notify super notifier).
    virtual void    notifyModified() {
        if ( getSuperProgress() != nullptr )
            getSuperProgress()->notifyModified();
    }

    /*! \brief Called at the start of the monitored process, indicate to an eventually associed progress display that it must be shown (call reset() automatically).
     *
     * \warning User should call base method at the begining of their implementation.
     */
    virtual void    beginProgress() { _active = true; }

    //! Return true between a call to beginProgress() and endProgress().
    bool            isActive( ) const { return _active; }

    /*! \brief Called at the end of the monitored process or if the process has been interrupted, indicate to an eventually associed progress display that it must be shut down.
     *
     * \warning User should call base method at the end of their implementation.
     */
    virtual void    endProgress() { _active = false; }

protected:
    bool    _active = false;

public:
    //! Set the previsible number of phase in the monitored task, it should be followed by a beginPhase() call.
    virtual void    setPhaseCount( int phaseCount ) { (void)phaseCount; }
    /*! \brief Start a new phase in the overall progress.
     *
     * \warning calling beginPhase() will automatically close the previous phase and update the overall progress
     * even if previous phase progress was not complete:
     *
     * \code
     *  // Setup a progress notifier with 2 phases, each weighting 50% of the global progress
     *  gtpo::ProgressNotifier progress;
     *  progress.setPhaseCount( 2 );
     *  progress.beginPhase();
     *  progress.setPhaseProgress( 0.5 );
     *  // progress.getProgress() == 0.25   (because phase 1 progress is 0.5 and phase weight is 0.5, global progress is 0.5*0.5=0.25
     *  progress.beginPhase( );
     *  // Warning: progress.getProgress() == 0.5, it now equal previous phase total weight even if the phase was not manually complete with setProgressPhase(1.0)
     *  progress.setPhaseProgress( 0.5 );
     *  // progress.getProgress() == 0.75 (one could expect 0.5*0.5 + 0.5*0.5=0.5, but using nextPhase has automatically complete phase 1, so
     *  // progress is (phase1.progress = 1. * 0.5) + (0.5 * 0.5) = 0.75
     * \endcode
     *
     * \note The total number of calls to nextPhase() must equal the value of \c phaseCount set when beginning startup monitoring with setPhaseCount().
     */
    virtual void    beginPhase( const std::string& phaseLabel = "" ) { (void)phaseLabel; }
    //! Return the current phase number (int between 0 and phaseCount - 1).
    virtual int     getPhase() const { return 0; }
    //! Return current phase label.
    virtual const std::string&    getPhaseLabel() const { return _phaseLabel; }
    //! Return current phase progress into the (0.0, 1.0) range.
    virtual double  getPhaseProgress() const { return 0.; }
    //! Set the current phase progress (\c phaseProgress must be in the (0.0, 1.0) range.
    virtual void    setPhaseProgress( double phaseProgress ) { (void)phaseProgress; }
    //! Used to set the current phase label from a sub notifier, from a super progress notifier, use beginPhase().
    virtual void    setPhaseLabel( const std::string& phaseLabel ) {
        _phaseLabel = phaseLabel;
        notifyModified();
    }

    //! Return the number of phase count in this notifier, if the notifier has sub notifiers, phase count will be the sum of all notifiers phase count.
    virtual int     getPhaseCount() const { return 1; }

protected:
    //! Current phase label.
    std::string _phaseLabel         = "";
};

//! Multi levels, multi phases progress notifier (basic concrete implementation of IProgressNotifier).
class ProgressNotifier : public IProgressNotifier
{
public:
    ProgressNotifier( IProgressNotifier* superProgress = nullptr ) :
        IProgressNotifier( superProgress ) { }

public:
    //! \copydoc IProgressNotifier::reset()
    virtual void                reset() override {
        IProgressNotifier::reset( );
        _phaseProgress  = 0.;
        _phase          = 0;
        _phaseCount     = 1;
        notifyModified();
    }

    virtual IProgressNotifier*  createSubProgress() {
        ProgressNotifier* subProgress = new ProgressNotifier( this );
        IProgressNotifierPtr progressPtr( subProgress );
        _notifiers.emplace_back( std::move( progressPtr ) );
        return subProgress;
    }

public:
    virtual void    beginProgress() override {
        IProgressNotifier::beginProgress();
        if ( getSuperProgress() != nullptr )    // Force super progress begin when begin is called on a sub progress
            getSuperProgress()->beginProgress();
    }

public:
    /*! \copybrief IProgressNotifier::getProgress()
     *
     * When a sub progress notifier is attached to this progress notifier, getProgress()
     * return a global progress valid for all progress notifier, otherwise, it return the
     * notifier "local" progress.
     */
    virtual double  getProgress() const override {
        double totalPhaseCount = getPhaseCount();
        double phaseWeight = ( 1.0 / _phaseCount );
        double progress = 0.;
        if ( _phase >= 0 ) {
            progress = ( _phase * phaseWeight ) + ( _phaseProgress * phaseWeight );
            progress *= _phaseCount / totalPhaseCount;
        }
        for ( auto& subNotifier : _notifiers ) {
            progress += subNotifier->getProgress() * subNotifier->getPhaseCount( ) / totalPhaseCount;
        }
        return progress;
    }
public:
    //! \copydoc IProgressNotifier::setPhaseCount()
    virtual void    setPhaseCount( int phaseCount ) override {
        _phaseLabel = "";
        _phaseCount = phaseCount;
        _phase = -1;
        _phaseProgress = 0.;
        if ( getSuperProgress() != nullptr )
            getSuperProgress()->update();
        notifyModified();
    }

    //! \copydoc IProgressNotifier::getPhaseCount()
    virtual int     getPhaseCount() const {
        int phaseCount = _phaseCount;
        for ( auto& subNotifier : _notifiers )
            phaseCount += subNotifier->getPhaseCount();
        return phaseCount;
    }

    //! \copydoc IProgressNotifier::beginPhase()
    virtual void    beginPhase( const std::string& phaseLabel = "" ) override {
        _phaseLabel = phaseLabel;
        _phase++;
        _phaseProgress = 0.;
        notifyModified();
    }

    //! \copydoc IProgressNotifier::getPhase()
    virtual int     getPhase( ) const override { return _phase; }

    //! \copydoc IProgressNotifier::getPhaseLabel()
    virtual const std::string&    getPhaseLabel() const override {
        for ( auto& subNotifier : _notifiers )
            if ( subNotifier->isActive() )
                return subNotifier->getPhaseLabel();
        return _phaseLabel;
    }

    //! \copydoc IProgressNotifier::getPhaseProgress()
    virtual double  getPhaseProgress() const override {
        for ( auto& subNotifier : _notifiers )
            if ( subNotifier->isActive() )
                return subNotifier->getPhaseProgress();
        return _phaseProgress;
    }

    //! \copydoc IProgressNotifier::setPhaseProgress()
    virtual void    setPhaseProgress( double phaseProgress ) override {
        _phaseProgress = phaseProgress;
        notifyModified();
    }

private:
    double      _phaseProgress      = 0.;
    //! Current phase index.
    int         _phase              = 0;
    //! Number of phase registered with setPhaseCount().
    int         _phaseCount         = 1;
};


/*! \brief Report progress in an output stream (default to std::cerr).
 *
 * Example use for debugging purposes:
 * \code
 *    FIXME
 * \endcode
 */
class EchoProgressNotifier : public ProgressNotifier
{
public:
    EchoProgressNotifier( IProgressNotifier* superProgress = nullptr ) :
        ProgressNotifier( superProgress ) { }

public:
    virtual IProgressNotifier*  createSubProgress() {
        EchoProgressNotifier* subProgress = new EchoProgressNotifier( this );
        IProgressNotifierPtr progressPtr( subProgress );
        _notifiers.emplace_back( std::move( progressPtr ) );
        return subProgress;
    }

    virtual void    notifyModified() override {
        std::cout << "Progress=" << getProgress() << std::endl;
        std::cout << "Phase=" << getPhase() << std::endl;
        std::cout << "Phase progress=" << getProgress() << std::endl;
    }
};

} // ::gtpo

#endif // gtpoProgressNotifier_h
