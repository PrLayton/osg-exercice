/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2003 Robert Osfield 
 *
 * This library is open source and may be redistributed and/or modified under  
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or 
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * OpenSceneGraph Public License for more details.
*/

#include <osgProducer/Viewer>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/ImageOptions>

#include <osgUtil/Optimizer>

#include <osg/Geode>
#include <osg/Notify>
#include <osg/MatrixTransform>
#include <osg/Switch>
#include <osg/TexMat>
#include <osg/Texture2D>
#include <osg/PolygonOffset>

#include <osgText/Text>

#include <sstream>

class ImageReaderWriter : public osgDB::ReaderWriter
{
    public:
        virtual const char* className() { return "Image Reader"; }
        
        
        struct DataReference
        {
            DataReference():
                _fileName(),
                _resolutionX(256),
                _resolutionY(256),
                _center(0.625f,0.0f,0.0f),
                _maximumWidth(1.25f,0.0f,0.0f),
                _maximumHeight(0.0f,0.0f,1.0f),
                _numPointsAcross(10), 
                _numPointsUp(10) {}

            DataReference(const std::string& fileName, unsigned int res, float width, float height):
                _fileName(fileName),
                _resolutionX(res),
                _resolutionY(res),
                _center(width*0.5f,0.0f,height*0.5f),
                _maximumWidth(width,0.0f,0.0f),
                _maximumHeight(0.0f,0.0f,height),
                _numPointsAcross(10), 
                _numPointsUp(10) {}
        
            DataReference(const DataReference& rhs):
                _fileName(rhs._fileName),
                _resolutionX(rhs._resolutionX),
                _resolutionY(rhs._resolutionY),
                _center(rhs._center),
                _maximumWidth(rhs._maximumWidth),
                _maximumHeight(rhs._maximumHeight),
                _numPointsAcross(rhs._numPointsAcross), 
                _numPointsUp(rhs._numPointsUp) {}

            std::string     _fileName;
            unsigned int    _resolutionX;
            unsigned int    _resolutionY;
            osg::Vec3       _center;
            osg::Vec3       _maximumWidth; 
            osg::Vec3       _maximumHeight;
            unsigned int    _numPointsAcross; 
            unsigned int    _numPointsUp;
        };
        
        typedef std::map<std::string,DataReference> DataReferenceMap;
        DataReferenceMap _dataReferences;
        
        std::string insertReference(const std::string& fileName, unsigned int res, float width, float height)
        {
	    std::stringstream ostr;
	    ostr<<"res_"<<res<<"_"<<fileName;

            std::string myReference = ostr.str();
            _dataReferences[myReference] = DataReference(fileName,res,width,height);
            return myReference;
        }
        
        

        virtual ReadResult readNode(const std::string& fileName, const Options* opt)
        {
            std::cout<<"Trying to read paged image "<<fileName<<std::endl;
            
            DataReferenceMap::iterator itr = _dataReferences.find(fileName);
            if (itr==_dataReferences.end()) return ReaderWriter::ReadResult::FILE_NOT_HANDLED;

            DataReference& dr = itr->second;
            
            // record previous options.
            osg::ref_ptr<osgDB::ReaderWriter::Options> previousOptions = osgDB::Registry::instance()->getOptions();

            osg::ref_ptr<osgDB::ImageOptions> options = new osgDB::ImageOptions;
            options->_destinationImageWindowMode = osgDB::ImageOptions::PIXEL_WINDOW;
            options->_destinationPixelWindow.set(0,0,dr._resolutionX,dr._resolutionY);

            osgDB::Registry::instance()->setOptions(options.get());
            
            osg::Image* image = osgDB::readImageFile(dr._fileName);
            
            // restore previous options.
            osgDB::Registry::instance()->setOptions(previousOptions.get());

            if (image)
            {
            
                float s = options.valid()?options->_sourcePixelWindow.windowWidth:1.0f;
                float t = options.valid()?options->_sourcePixelWindow.windowHeight:1.0f;
            
                float photoWidth = 0.0f;
                float photoHeight = 0.0f;
                float maxWidth = dr._maximumWidth.length();
                float maxHeight = dr._maximumHeight.length();
                
                
                if ((s/t)>(maxWidth/maxHeight))
                {
                    // photo wider than tall relative to the required pictures size.
                    // so need to clamp the width to the maximum width and then
                    // set the height to keep the original photo aspect ratio.
                    
                    photoWidth = maxWidth;
                    photoHeight = photoWidth*(t/s);
                }
                else
                {
                    // photo tall than wide relative to the required pictures size.
                    // so need to clamp the height to the maximum height and then
                    // set the width to keep the original photo aspect ratio.
                    
                    photoHeight = maxHeight;
                    photoWidth = photoHeight*(s/t);
                }
                
                osg::Vec3 halfWidthVector(dr._maximumWidth*(photoWidth*0.5f/maxWidth));
                osg::Vec3 halfHeightVector(dr._maximumHeight*(photoHeight*0.5f/maxHeight));


                // set up the texture.
                osg::Texture2D* texture = new osg::Texture2D;
                texture->setImage(image);
                texture->setFilter(osg::Texture::MIN_FILTER,osg::Texture::LINEAR);
                texture->setFilter(osg::Texture::MAG_FILTER,osg::Texture::LINEAR);

                // set up the drawstate.
                osg::StateSet* dstate = new osg::StateSet;
                dstate->setMode(GL_LIGHTING,osg::StateAttribute::OFF);
                dstate->setTextureAttributeAndModes(0, texture,osg::StateAttribute::ON);

                // set up the geoset.
                osg::Geometry* geom = new osg::Geometry;
                geom->setStateSet(dstate);

                osg::Vec3Array* coords = new osg::Vec3Array(4);
                (*coords)[0] = dr._center - halfWidthVector + halfHeightVector;
                (*coords)[1] = dr._center - halfWidthVector - halfHeightVector;
                (*coords)[2] = dr._center + halfWidthVector - halfHeightVector;
                (*coords)[3] = dr._center + halfWidthVector + halfHeightVector;
                geom->setVertexArray(coords);

                osg::Vec2Array* tcoords = new osg::Vec2Array(4);
                (*tcoords)[0].set(0.0f,1.0f);
                (*tcoords)[1].set(0.0f,0.0f);
                (*tcoords)[2].set(1.0f,0.0f);
                (*tcoords)[3].set(1.0f,1.0f);
                geom->setTexCoordArray(0,tcoords);

                osg::Vec4Array* colours = new osg::Vec4Array(1);
                (*colours)[0].set(1.0f,1.0f,1.0,1.0f);
                geom->setColorArray(colours);
                geom->setColorBinding(osg::Geometry::BIND_OVERALL);

                geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS,0,4));

                // set up the geode.
                osg::Geode* geode = new osg::Geode;
                geode->addDrawable(geom);
                
                return geode;
            
            }
            else
            {
                return ReaderWriter::ReadResult::FILE_NOT_HANDLED;
            }
            
                        
        }

};


// now register with Registry to instantiate the above
// reader/writer.
osgDB::RegisterReaderWriterProxy<ImageReaderWriter> g_ImageReaderWriter;

class Album;

class Page : public osg::Transform
{
public:


    static Page* createPage(Album* album, unsigned int pageNo, const std::string& filename, float width, float height)
    {
        osg::ref_ptr<Page> page = new Page(album, pageNo, filename, width, height);
        if (page.valid()) return page.release();
        else return 0;
    }
    
    virtual void traverse(osg::NodeVisitor& nv);

    void setRotation(float angle)
    {
        _rotation = angle; 
        _targetRotation = angle; 
        dirtyBound();
    }

    float getRotation() const { return _rotation; }

    void rotateTo(float angle, float timeToRotateBy)
    {
        _targetRotation = angle; 
        _targetTime = timeToRotateBy;
    }
    
    bool rotating() const { return _targetRotation!=_rotation; }

    void setPageVisible(bool visible) { _switch->setSingleChildOn(visible?1:0); }

    osg::Switch* getSwitch() { return _switch.get(); }
    const osg::Switch* getSwitch() const { return _switch.get(); }

public:

    virtual bool computeLocalToWorldMatrix(osg::Matrix& matrix,osg::NodeVisitor*) const 
    {
        if (_referenceFrame==RELATIVE_TO_PARENTS)
        {
            matrix.preMult(getMatrix());
        }
        else // absolute
        {
            matrix = getMatrix();
        }
        return true;
    }

    /** Get the transformation matrix which moves from world coords to local coords.*/
    virtual bool computeWorldToLocalMatrix(osg::Matrix& matrix,osg::NodeVisitor*) const
    {
        const osg::Matrix& inverse = getInverseMatrix();

        if (_referenceFrame==RELATIVE_TO_PARENTS)
        {
            matrix.postMult(inverse);
        }
        else // absolute
        {
            matrix = inverse;
        }
        return true;
    }

    osg::Matrix getMatrix() const { return _pageOffset*osg::Matrix::rotate(-_rotation,0.0f,0.0f,1.0f); }
    osg::Matrix getInverseMatrix() const { return osg::Matrix::inverse(getMatrix()); }

protected:
    
    Page(Album* album, unsigned int pageNo, const std::string& filename, float width, float height);

    float       _rotation;
    osg::Matrix _pageOffset;

    float       _targetRotation;
    float       _targetTime;
    float       _lastTimeTraverse;

    osg::ref_ptr<osg::Switch>     _switch;

};


class Album : public osg::Referenced
{
public:

    Album(osg::ArgumentParser& ap, float width, float height);

    osg::Group* getScene() { return _group.get(); }
    
    const osg::Group* getScene() const { return _group.get(); }

    osg::Matrix getPageOffset(unsigned int pageNo) const;
    
    bool nextPage(float timeToRotateBy) { return gotoPage(_currentPageNo+1,timeToRotateBy); }

    bool previousPage(float timeToRotateBy) { return _currentPageNo>=1?gotoPage(_currentPageNo-1,timeToRotateBy):false; }
    
    bool gotoPage(unsigned int pageNo, float timeToRotateBy);
    
    osg::StateSet* getBackgroundStateSet() { return _backgroundStateSet.get(); }
    
    void setVisibility();

protected:

    typedef std::vector< osg::ref_ptr<Page> > PageList;

    osg::ref_ptr<osg::Group>    _group;
    PageList                    _pages;
    
    osg::ref_ptr<osg::StateSet> _backgroundStateSet;
    
    unsigned int                _currentPageNo;
    float                       _radiusOfRings;
    float                       _startAngleOfPages;
    float                       _deltaAngleBetweenPages;   

};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


Page::Page(Album* album, unsigned int pageNo, const std::string& filename, float width, float height)
{
    // set up transform parts.
    _rotation = 0;
    _targetRotation = 0;
    _targetTime = 0;
    _lastTimeTraverse = 0;
    
    _pageOffset = album->getPageOffset(pageNo);
    
    setNumChildrenRequiringUpdateTraversal(1);
    
    
    // set up subgraph
    osgDB::ReaderWriter* readerWriter = osgDB::Registry::instance()->getReaderWriterForExtension("gdal");
    if (!readerWriter)
    {
        std::cout<<"Error: GDAL plugin not available, cannot preceed with database creation"<<std::endl;
    }

    _switch = new osg::Switch;

    ImageReaderWriter* rw = g_ImageReaderWriter.get();

    
    // set up non visible page.
    osg::Group* non_visible_page = new osg::Group;
    _switch->addChild(non_visible_page);
    {
        // just an empty group for the time being... will need to create geometry soon.
        osg::Geometry* geom = new osg::Geometry;
        geom->setStateSet(album->getBackgroundStateSet());

        osg::Vec3Array* coords = new osg::Vec3Array(4);
        (*coords)[0].set(0.0f,0.0,height);
        (*coords)[1].set(0.0f,0.0,0);
        (*coords)[2].set(width,0.0,0);
        (*coords)[3].set(width,0.0,height);
        geom->setVertexArray(coords);

        osg::Vec3Array* normals = new osg::Vec3Array(1);
        (*normals)[0].set(0.0f,-1.0f,0.0f);
        geom->setNormalArray(normals);
        geom->setNormalBinding(osg::Geometry::BIND_OVERALL);

        osg::Vec2Array* tcoords = new osg::Vec2Array(4);
        (*tcoords)[0].set(0.0f,1.0f);
        (*tcoords)[1].set(0.0f,0.0f);
        (*tcoords)[2].set(1.0f,0.0f);
        (*tcoords)[3].set(1.0f,1.0f);
        geom->setTexCoordArray(0,tcoords);

        osg::Vec4Array* colours = new osg::Vec4Array(1);
        (*colours)[0].set(1.0f,1.0f,1.0,1.0f);
        geom->setColorArray(colours);
        geom->setColorBinding(osg::Geometry::BIND_OVERALL);

        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_LOOP,0,4));

        // set up the geode.
        osg::Geode* geode = new osg::Geode;
        geode->addDrawable(geom);
        
    
        non_visible_page->addChild(geode);
    }


    // set up visible page.
    osg::Group* visible_page = new osg::Group;
    _switch->addChild(visible_page);

    {

        
        osg::Geometry* geom = new osg::Geometry;
        geom->setStateSet(album->getBackgroundStateSet());

        osg::Vec3Array* coords = new osg::Vec3Array(4);
        (*coords)[0].set(0.0f,0.0,height);
        (*coords)[1].set(0.0f,0.0,0);
        (*coords)[2].set(width,0.0,0);
        (*coords)[3].set(width,0.0,height);
        geom->setVertexArray(coords);

        osg::Vec3Array* normals = new osg::Vec3Array(1);
        (*normals)[0].set(0.0f,-1.0f,0.0f);
        geom->setNormalArray(normals);
        geom->setNormalBinding(osg::Geometry::BIND_OVERALL);

        osg::Vec2Array* tcoords = new osg::Vec2Array(4);
        (*tcoords)[0].set(0.0f,1.0f);
        (*tcoords)[1].set(0.0f,0.0f);
        (*tcoords)[2].set(1.0f,0.0f);
        (*tcoords)[3].set(1.0f,1.0f);
        geom->setTexCoordArray(0,tcoords);

        osg::Vec4Array* colours = new osg::Vec4Array(1);
        (*colours)[0].set(1.0f,1.0f,1.0,1.0f);
        geom->setColorArray(colours);
        geom->setColorBinding(osg::Geometry::BIND_OVERALL);

        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS,0,4));

        // set up the geode.
        osg::Geode* geode = new osg::Geode;
        geode->addDrawable(geom);
        
    
        visible_page->addChild(geode);
    }

    {
        float cut_off_distance = 8.0f;
        float max_visible_distance = 300.0f;
        
        osg::Vec3 center(width*0.5f,0.0f,height*0.5f);

        osgText::Text* text = new osgText::Text;
        text->setFont("fonts/arial.ttf");
        text->setPosition(center);
        text->setCharacterSize(height/20.0f);
        text->setAlignment(osgText::Text::CENTER_CENTER);
        text->setAxisAlignment(osgText::Text::XZ_PLANE);
        text->setColor(osg::Vec4(1.0f,1.0f,0.0f,1.0f));
        text->setText(std::string("Loading ")+filename);

        osg::Geode* geode = new osg::Geode;
        geode->addDrawable(text);
        
        osg::PagedLOD* pagedlod = new osg::PagedLOD;
        pagedlod->setCenter(center);
        pagedlod->setRadius(1.6f);
        pagedlod->setNumChildrenThatCannotBeExpired(2);
        
        pagedlod->setRange(0,max_visible_distance,1e7);
        pagedlod->addChild(geode);
        
        pagedlod->setRange(1,cut_off_distance,max_visible_distance);
        pagedlod->setFileName(1,rw->insertReference(filename,256,width,height));

        pagedlod->setRange(2,0.0f,cut_off_distance);
        pagedlod->setFileName(2,rw->insertReference(filename,1024,width,height));

        visible_page->addChild(pagedlod);
    }
    
    addChild(_switch.get());
}

void Page::traverse(osg::NodeVisitor& nv)
{
    // if app traversal update the frame count.
    if (nv.getVisitorType()==osg::NodeVisitor::UPDATE_VISITOR)
    {
        const osg::FrameStamp* framestamp = nv.getFrameStamp();
        if (framestamp)
        {
            double t = framestamp->getReferenceTime();
            
            if (_rotation!=_targetRotation)
            {
                if (t>=_targetTime) _rotation = _targetRotation;
                else _rotation += (_targetRotation-_rotation)*(t-_lastTimeTraverse)/(_targetTime-_lastTimeTraverse);
                
                dirtyBound();
            }
            
            _lastTimeTraverse = t;

        }
    }
    Transform::traverse(nv);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Album::Album(osg::ArgumentParser& arguments, float width, float height)
{


    typedef std::vector<std::string> FileList;
    FileList fileList;

    for(int pos=1;pos<arguments.argc();++pos)
    {
        if (arguments.isString(pos)) fileList.push_back(arguments[pos]);
    }
    
    _radiusOfRings = 0.02;
    _startAngleOfPages = 0.0f;
    _deltaAngleBetweenPages = osg::PI/(float)fileList.size();
    
    _group = new osg::Group;
    
    _backgroundStateSet = new osg::StateSet;
    _backgroundStateSet->setAttributeAndModes(new osg::PolygonOffset(1.0f,1.0f),osg::StateAttribute::ON);
    
    // load the images.
    unsigned int i;
    for(i=0;i<fileList.size();++i)
    {
        Page* page = Page::createPage(this,_pages.size(),fileList[i], width, height);
        if (page)
        {
            _pages.push_back(page);
            _group->addChild(page);
        }
    }
    
    setVisibility();

}

osg::Matrix Album::getPageOffset(unsigned int pageNo) const
{
    float angleForPage = _startAngleOfPages+_deltaAngleBetweenPages*(float)pageNo;
    osg::Vec3 delta(_radiusOfRings*sinf(angleForPage),-_radiusOfRings*cosf(angleForPage),0.0f);
    return osg::Matrix::translate(delta);
}

bool Album::gotoPage(unsigned int pageNo, float timeToRotateBy)
{
    if (pageNo>=_pages.size()) return false;

    if (pageNo>_currentPageNo)
    {
        for(unsigned int i=_currentPageNo;i<pageNo;++i)
        {
            _pages[i]->rotateTo(osg::PI,timeToRotateBy);
        }
        _pages[pageNo]->setPageVisible(true);
        _currentPageNo = pageNo;
        
        return true;
    }
    else if (pageNo<_currentPageNo)
    {
        for(unsigned int i=pageNo;i<_currentPageNo;++i)
        {
            _pages[i]->rotateTo(0,timeToRotateBy);
        }
        _pages[pageNo]->setPageVisible(true);
        _currentPageNo = pageNo;
        
        return true;
    }
    
    return false;
}

void Album::setVisibility()
{
    for(unsigned int i=0;i<_pages.size();++i)
    {
        _pages[i]->setPageVisible(_pages[i]->rotating());
    }
    
    //_pages[0]->setPageVisible(true);
    //if (_currentPageNo>=1) _pages[_currentPageNo-1]->setPageVisible(true);
    _pages[_currentPageNo]->setPageVisible(true);
    //if (_currentPageNo<_pages.size()-1) _pages[_currentPageNo+1]->setPageVisible(true);
    //_pages[_pages.size()-1]->setPageVisible(true);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class SlideEventHandler : public osgGA::GUIEventHandler
{
public:

    SlideEventHandler();
    
    META_Object(osgStereImageApp,SlideEventHandler);

    void set(Album* album, float timePerSlide, bool autoSteppingActive);

    virtual void accept(osgGA::GUIEventHandlerVisitor& v) { v.visit(*this); }

    virtual bool handle(const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter&);
    
    virtual void getUsage(osg::ApplicationUsage& usage) const;

protected:

    ~SlideEventHandler() {}
    SlideEventHandler(const SlideEventHandler&,const osg::CopyOp&) {}

    osg::ref_ptr<Album>         _album;
    bool                        _firstTraversal;
    double                      _previousTime;
    double                      _timePerSlide;
    bool                        _autoSteppingActive;
};

SlideEventHandler::SlideEventHandler():
    _album(0),
    _firstTraversal(true),
    _previousTime(-1.0f),
    _timePerSlide(5.0),
    _autoSteppingActive(false)
{
}

void SlideEventHandler::set(Album* album, float timePerSlide, bool autoSteppingActive)
{
    _album = album;

    _timePerSlide = timePerSlide;
    _autoSteppingActive = autoSteppingActive;    
    
}

bool SlideEventHandler::handle(const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter&)
{
    switch(ea.getEventType())
    {
        case(osgGA::GUIEventAdapter::KEYDOWN):
        {
            if (ea.getKey()=='a')
            {
                _autoSteppingActive = !_autoSteppingActive;
                _previousTime = ea.time();
                return true;
            }
            else if (ea.getKey()=='n')
            {
                _album->nextPage(ea.time()+1.0f);
                return true;
            }
            else if (ea.getKey()=='p')
            {
                _album->previousPage(ea.time()+1.0f);
                return true;
            }
            return false;
        }
        case(osgGA::GUIEventAdapter::FRAME):
        {
            if (_autoSteppingActive)
            {
                if (_firstTraversal)
                {
                    _firstTraversal = false;
                    _previousTime = ea.time();
                }
                else if (ea.time()-_previousTime>_timePerSlide)
                {
                    _previousTime = ea.time();

                    _album->nextPage(ea.time()+1.0f);
                }
            }
            
            _album->setVisibility();

        }

        default:
            return false;
    }
}

void SlideEventHandler::getUsage(osg::ApplicationUsage& usage) const
{
    usage.addKeyboardMouseBinding("Space","Reset the image position to center");
    usage.addKeyboardMouseBinding("a","Toggle on/off the automatic advancement for image to image");
    usage.addKeyboardMouseBinding("n","Advance to next image");
    usage.addKeyboardMouseBinding("p","Move to previous image");
}

int main( int argc, char **argv )
{

    // use an ArgumentParser object to manage the program arguments.
    osg::ArgumentParser arguments(&argc,argv);
    
    // set up the usage document, in case we need to print out how to use this program.
    arguments.getApplicationUsage()->setDescription(arguments.getApplicationName()+" is the example which demonstrates use node masks to create stereo images.");
    arguments.getApplicationUsage()->setCommandLineUsage(arguments.getApplicationName()+" [options] image_file [image_file]");
    arguments.getApplicationUsage()->addCommandLineOption("-d <float>","Time delay in sceonds between the display of successive image pairs when in auto advance mode.");
    arguments.getApplicationUsage()->addCommandLineOption("-a","Enter auto advance of image pairs on start up.");
    arguments.getApplicationUsage()->addCommandLineOption("-h or --help","Display this information");
    

    // construct the viewer.
    osgProducer::Viewer viewer(arguments);

    // set up the value with sensible default event handlers.
    //viewer.setUpViewer(osgProducer::Viewer::ESCAPE_SETS_DONE);
    viewer.setUpViewer();

    // register the handler to add keyboard and mosue handling.
    SlideEventHandler* seh = new SlideEventHandler();
    viewer.getEventHandlerList().push_front(seh);


    // get details on keyboard and mouse bindings used by the viewer.
    viewer.getUsage(*arguments.getApplicationUsage());

    // read any time delay argument.
    float timeDelayBetweenSlides = 5.0f;
    while (arguments.read("-d",timeDelayBetweenSlides)) {}

    bool autoSteppingActive = false;
    while (arguments.read("-a")) autoSteppingActive = true;

    // if user request help write it out to cout.
    if (arguments.read("-h") || arguments.read("--help"))
    {
        arguments.getApplicationUsage()->write(std::cout);
        return 1;
    }

    // any option left unread are converted into errors to write out later.
    arguments.reportRemainingOptionsAsUnrecognized();

    // report any errors if they have occured when parsing the program aguments.
    if (arguments.errors())
    {
        arguments.writeErrorMessages(std::cout);
        return 1;
    }
    
    if (arguments.argc()<=1)
    {
        arguments.getApplicationUsage()->write(std::cout,osg::ApplicationUsage::COMMAND_LINE_OPTION);
        return 1;
    }



    // now the windows have been realized we switch off the cursor to prevent it
    // distracting the people seeing the stereo images.
    float fovx = 1.25f;
    float fovy = 1.0f;
    for( unsigned int i = 0; i < viewer.getCameraConfig()->getNumberOfCameras(); i++ )
    {
        Producer::Camera* cam = viewer.getCameraConfig()->getCamera(i);
        Producer::RenderSurface* rs = cam->getRenderSurface();
        //rs->useCursor(false);
        fovx = cam->getLensHorizontalFov();
        fovy = cam->getLensVerticalFov();
    }

    float radius = 1.0f;
    float width = 2*radius*tan(fovx*0.5f);
    float height = 2*radius*tan(fovy*0.5f);

    osg::ref_ptr<Album> album = new Album(arguments,width,height);

    // creat the scene from the file list.
    osg::ref_ptr<osg::Group> rootNode = album->getScene();
    
    if (!rootNode) return 0;


    //osgDB::writeNodeFile(*rootNode,"test.osg");

    // set the scene to render
    viewer.setSceneData(album->getScene());


    // set up the SlideEventHandler.
    seh->set(album.get(),timeDelayBetweenSlides,autoSteppingActive);
    

    // create the windows and run the threads.
    viewer.realize();
    
    osg::Matrix homePosition;
    homePosition.makeLookAt(osg::Vec3(0.0f,0.0f,0.0f),osg::Vec3(0.0f,1.0f,0.0f),osg::Vec3(0.0f,0.0f,1.0f));
        
    while( !viewer.done() )
    {
        // wait for all cull and draw threads to complete.
        viewer.sync();

        // update the scene by traversing it with the the update visitor which will
        // call all node update callbacks and animations.
        viewer.update();
         
        //viewer.setView(homePosition);

        // fire off the cull and draw traversals of the scene.
        viewer.frame();
        
    }
    
    // wait for all cull and draw threads to complete before exit.
    viewer.sync();
    
    return 0;
}

