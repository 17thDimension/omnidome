/* Copyright (c) 2014-2015 "Omnidome" by cr8tr
 * Dome Mapping Projection Software (http://omnido.me).
 * Omnidome was created by Michael Winkelmann aka Wilston Oreo (@WilstonOreo)
 *
 * This file is part of Omnidome.
 *
 * Omnidome is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <omni/visual/Tuning.h>

#include <QVector4D>
#include <QOpenGLTexture>
#include <QOpenGLPixelTransferOptions>
#include <omni/util.h>
#include <omni/visual/util.h>
#include <omni/visual/Rectangle.h>
#include <omni/visual/Session.h>

namespace omni {
  namespace visual {
    std::unique_ptr<QOpenGLShaderProgram> Tuning::blendShader_;
    std::unique_ptr<QOpenGLShaderProgram> Tuning::testCardShader_;

    Tuning::Tuning(omni::proj::Tuning& _tuning) :
      tuning_(_tuning)
    {
      warpGrid_.reset(new visual::WarpGrid(tuning_.warpGrid()));
    }

    omni::proj::Tuning const& Tuning::tuning() const
    {
      return tuning_;
    }

    void Tuning::drawTestCard(int _id, bool _grayscale) const
    {
      if (!testCardShader_) return;

      if (!QOpenGLContext::currentContext()) return;

      visual::with_current_context([&](QOpenGLFunctions& _)
      {
        auto _color = tuning_.color();
        GLfloat _red = _color.redF();
        GLfloat _green = _color.greenF();
        GLfloat _blue = _color.blueF();

        testCardShader_->bind();
        testCardShader_->setUniformValue("resolution",
                                         GLfloat(tuning_.width()),
                                         GLfloat(tuning_.height()));
        testCardShader_->setUniformValue("test_color",   _red,
                                         _green,
                                         _blue);
        testCardShader_->setUniformValue("projector_id", _id);
        testCardShader_->setUniformValue("gray_output",  _grayscale);

        Rectangle::draw();
        testCardShader_->release();
      });
    }

    void Tuning::drawWarpGrid() const
    {
      if (!warpGrid_) return;

      visual::with_current_context([this](QOpenGLFunctions& _)
      {
        warpGrid_->drawLines();
        warpGrid_->drawHandles(tuning_.color(), tuningRect());
      });
    }

    void Tuning::drawWarpPatch() const
    {
      visual::with_current_context([this](QOpenGLFunctions& _)
      {
        _.glEnable(GL_TEXTURE_2D);
        _.glDisable(GL_BLEND);
        glColor4f(1.0, 1.0, 1.0, 1.0);
        warpGrid_->draw();
      });
    }

    void Tuning::update()
    {
      if (!QOpenGLContext::currentContext()) return;

      using omni::util::fileToStr;

      if (!cursor_) {
        cursor_.reset(new Circle);
      }

      if (!testCardShader_)
      {
        static QString _vertSrc =
          fileToStr(":/shaders/testcard.vert");
        static QString _fragmentSrc =
          fileToStr(":/shaders/testcard.frag");

        testCardShader_.reset(new QOpenGLShaderProgram());
        testCardShader_->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                 _vertSrc);
        testCardShader_->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                 _fragmentSrc);
        testCardShader_->link();
      }

      if (!blendShader_)
      {
        static QString _vertSrc     = fileToStr(":/shaders/blend.vert");
        static QString _fragmentSrc = fileToStr(":/shaders/blend.frag");
        blendShader_.reset(new QOpenGLShaderProgram());
        blendShader_->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                              _vertSrc);
        blendShader_->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                              _fragmentSrc);
        blendShader_->link();
      }

      updateWarpGrid();
      updateBlendTexture();
    }

    void Tuning::setBlendTextureUpdateRect(QRect const& _rect)
    {
      int _radius = tuning_.blendMask().brush().size() / 2 + 1;

      blendTextureUpdateRect_ = _rect.normalized().adjusted(-_radius,
                                                            -_radius,
                                                            _radius,
                                                            _radius)
                                & QRect(0, 0,
                                        tuning_.width(), tuning_.height());
    }

    void Tuning::updateBlendTexture()
    {
      if (!QOpenGLContext::currentContext()) return;

      visual::with_current_context([&](QOpenGLFunctions& _)
      {
        bool _reset = !blendTex_;

        if (!!blendTex_)
        {
          // Reset blend texture if its size has changed
          _reset = blendTex_->width() != tuning_.width() || blendTex_->height() != tuning_.height();
        }

        if (_reset)
        {
          // Reset blend texture
          blendTextureUpdateRect_ =
            QRect(0, 0, tuning_.width(), tuning_.height());
          blendTex_.reset(new QOpenGLTexture(tuning_.blendMask().
                                             strokeBuffer().toQImage()));
        }

        auto& _blendMask = tuning().blendMask();
        auto _ptr = _blendMask.strokeBufferData();

        auto _fullRect = QRect(0, 0, tuning_.width(), tuning_.height());
        auto& _r = blendTextureUpdateRect_;

        /// Optimization for uploading only a portion of the blend
        // buffer to texture
        if (_r != _fullRect)
        {
          auto _data = _blendMask.strokeBuffer().cropRect(_r);
          _ptr = (void *)(_data.data().data());
          _.glBindTexture(GL_TEXTURE_2D, blendTex_->textureId());
          _.glPixelStorei(GL_UNPACK_ALIGNMENT,   1);
          _.glPixelStorei(GL_UNPACK_ROW_LENGTH,  0);
          _.glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
          _.glPixelStorei(GL_UNPACK_SKIP_ROWS,   0);
          _.glTexSubImage2D(GL_TEXTURE_2D, 0, _r.x(), _r.y(),
                            _r.width(), _r.height(), GL_ALPHA,
                            GL_UNSIGNED_BYTE, _ptr);
          _.glBindTexture(GL_TEXTURE_2D, 0);
          blendTextureUpdateRect_ = _fullRect;
        } else
        {
          _.glBindTexture(GL_TEXTURE_2D, blendTex_->textureId());
          _.glTexSubImage2D(GL_TEXTURE_2D, 0, _r.x(), _r.y(),
                            _r.width(), _r.height(), GL_ALPHA,
                            GL_UNSIGNED_BYTE, _ptr);
          _.glBindTexture(GL_TEXTURE_2D, 0);
        }
      });
    }

    void Tuning::drawCursor(QPointF const& _pos)
    {
      if (!cursor_) return;

      auto _rect = tuningRect();

      visual::with_current_context([&](QOpenGLFunctions& _)
      {
        _.glDisable(GL_LINE_SMOOTH);
        _.glEnable(GL_BLEND);
        _.glEnable(GL_COLOR_LOGIC_OP);
        glLogicOp(GL_XOR);

        glColor4f(0.0, 0.0, 0.0, 1.0);
        glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR);

        float _r = tuning_.blendMask().brush().size() * 0.5 / tuning_.width();
        cursor_->drawLine(_pos, _r, _r * (_rect.height() / _rect.width()));
        _.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        _.glEnable(GL_LINE_SMOOTH);
        _.glDisable(GL_COLOR_LOGIC_OP);
      });
    }

    bool Tuning::initialized() const
    {
      return !!blendTex_ && !!blendShader_ && !!testCardShader_;
    }

    void Tuning::drawOutput(float         _inputOpacity,
                            QColor const& _color,
                            float         _blendMaskOpacity,
                            bool          _grayscale) const
    {
      if (!blendShader_ || !warpGridBuffer_) return;

      int _inputTexId = warpGridBuffer_->texture();

      visual::with_current_context([&](QOpenGLFunctions& _)
      {
        warpGrid_->draw();

        auto& _mask = tuning().blendMask();
        glBlendColor(1.0, 1.0, 1.0, 1.0);
        _.glEnable(GL_DEPTH_TEST);
        _.glDepthFunc(GL_LEQUAL);
        _.glEnable(GL_BLEND);
        _.glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        _.glBindTexture(GL_TEXTURE_2D, _inputTexId);
        _.glActiveTexture(GL_TEXTURE0);

        blendShader_->bind();
        {
          blendShader_->setUniformValue("top",
                                        _mask.topWidth());
          blendShader_->setUniformValue("right",
                                        _mask.
                                        rightWidth());
          blendShader_->setUniformValue("bottom",
                                        _mask.
                                        bottomWidth());
          blendShader_->setUniformValue("left",
                                        _mask.leftWidth());
          blendShader_->setUniformValue("edge_gamma",    _mask.gamma());
          blendShader_->setUniformValue("input_tex",     0);
          blendShader_->setUniformValue("input_opacity", _inputOpacity);
          blendShader_->setUniformValue("color",         _color.redF(),
                                        _color.greenF(),
                                        _color.blueF());
          blendShader_->setUniformValue("gray_output",   _grayscale);

          auto& _cC = tuning().colorCorrection();

          if (_cC.isUsed()) {
            blendShader_->setUniformValue("cc_gamma", QVector4D(
                                            _cC.red().gamma(),
                                            _cC.green().gamma(),
                                            _cC.blue().gamma(),
                                            _cC.all().gamma()));
            blendShader_->setUniformValue("cc_brightness", QVector4D(
                                            _cC.red().brightness(),
                                            _cC.green().brightness(),
                                            _cC.blue().brightness(),
                                            _cC.all().brightness()));
            blendShader_->setUniformValue("cc_contrast", QVector4D(
                                            _cC.red().contrast(),
                                            _cC.green().contrast(),
                                            _cC.blue().contrast(),
                                            _cC.all().contrast()));
            blendShader_->setUniformValue("cc_multiplier", QVector4D(
                                            _cC.red().multiplier(),
                                            _cC.green().multiplier(),
                                            _cC.blue().multiplier(),
                                            _cC.all().multiplier()));
          } else {
            QVector4D _null(0.0, 0.0, 0.0, 0.0);
            blendShader_->setUniformValue("cc_multiplier", _null);
            blendShader_->setUniformValue("cc_contrast",   _null);
            blendShader_->setUniformValue("cc_brightness", _null);
            blendShader_->setUniformValue("cc_gamma",      _null);
          }


          blendShader_->setUniformValue("mask",
                                        GLfloat(_blendMaskOpacity));
          warpGrid_->draw();
        }
        blendShader_->release();
        _.glBindTexture(GL_TEXTURE_2D, 0);


        if (_blendMaskOpacity > 0.0) {
          _.glDisable(GL_BLEND);
          glColor4f(0.0, 0.0, 0.0, 1.0);

          float _b = 4.0;

          // Draw masks for borders
          glBegin(GL_QUADS);
          {
            glVertex2f(-0.5,      -0.5 - _b);
            glVertex2f(0.5 - _b,  -0.5);
            glVertex2f(0.5 - _b,  0.5);
            glVertex2f(-0.5,      0.5 + _b);
            glVertex2f(-0.5 + _b, -0.5 - _b);
            glVertex2f(0.5,       -0.5);
            glVertex2f(0.5,       0.5);
            glVertex2f(-0.5 + _b, 0.5 + _b);
            glVertex2f(-0.5 + _b, 0.5 + _b);
            glVertex2f(0.5 - _b,  0.5 + _b);
            glVertex2f(0.5 - _b,  0.5);
            glVertex2f(-0.5 + _b, 0.5);
            glVertex2f(-0.5 + _b, -0.5);
            glVertex2f(0.5 - _b,  -0.5);
            glVertex2f(0.5 - _b,  -0.5 - _b);
            glVertex2f(-0.5 + _b, -0.5 - _b);
          }
          glEnd();

          _.glEnable(GL_BLEND);
          _.glEnable(GL_TEXTURE_2D);
          _.glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
          blendTex_->bind();
          {
            glColor4f(0.0, 0.0, 0.0, 1.0);
            glTexEnvf(GL_TEXTURE_ENV,
                      GL_TEXTURE_ENV_MODE,
                      GL_MODULATE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                            GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                            GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                            GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR);

            Rectangle::drawFlipped();
          }
          blendTex_->release();
        }
      });
    }

    void Tuning::updateWarpGrid() {
      if (!warpGrid_) return;

      warpGrid_->update();
    }

    /// Update warp buffer which contains image of projector perspective
    void Tuning::updateWarpBuffer(visual::Session const* _vizSession)
    {
      // If tuning size has changed, reset warpGrid framebuffer
      if (warpGridBuffer_)
      {
        if ((warpGridBuffer_->width() != tuning().width()) ||
            (warpGridBuffer_->height() !=
             tuning().height())) warpGridBuffer_.reset();
      }

      // If warp grid framebuffer is empty, make a new one
      if (!warpGridBuffer_)
      {
        warpGridBuffer_.reset(new QOpenGLFramebufferObject(
                                tuning().width(),
                                tuning().height()));
        warpGridBuffer_->setAttachment(QOpenGLFramebufferObject::Depth);
      }

      // Draw projector's perspective on framebuffer texture
      with_current_context([&](QOpenGLFunctions& _)
      {
        _.glEnable(GL_TEXTURE_2D);
        _.glDisable(GL_LIGHTING);
        draw_on_framebuffer(warpGridBuffer_,
                                    [&](QOpenGLFunctions& _) // Projection
                                                             // Operation
        {
          glMultMatrixf(tuning().projector().projectionMatrix().constData());
        },
                                    [&](QOpenGLFunctions& _) // Model View
                                                             // Operation
        {
          _.glClearColor(0.0, 0.0, 0.0, 1.0);
          _.glClear(GL_DEPTH_BUFFER_BIT);

          _.glEnable(GL_DEPTH_TEST);
          _.glDepthFunc(GL_LEQUAL);
          _.glEnable(GL_BLEND);
          _.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
          _.glEnable(GL_LINE_SMOOTH);
          _.glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
          _.glEnable(GL_POINT_SMOOTH);
          _.glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
          _.glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
          glPolygonMode(GL_FRONT, GL_FILL);
          glPolygonMode(GL_BACK, GL_FILL);
          _.glEnable(GL_NORMALIZE);

          // fix outlines z-fighting with quads
          _.glPolygonOffset(1, 1);

          _.glEnable(GL_DEPTH_TEST);
          _vizSession->drawCanvas();
        });
      });
    }

    void Tuning::free()
    {
      if (!!blendTex_)
      {
        blendTex_->destroy();
        blendTex_.reset();
      }

      blendShader_.reset();
      testCardShader_.reset();
    }

    QRectF Tuning::tuningRect() const
    {
      float _projAspect = float(tuning_.width()) / tuning_.height();
      float _left       = -0.5, _right = 0.5, _bottom = -0.5, _top = 0.5;

      if (_projAspect > 1.0)
      {
        _top    *= _projAspect;
        _bottom *=  _projAspect;
      }
      else
      {
        _left  /= _projAspect;
        _right /= _projAspect;
      }
      return QRectF(QPointF(_left, _top), QPointF(_right, _bottom));
    }
  }
}
