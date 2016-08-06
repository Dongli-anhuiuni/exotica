#ifndef PROPERTY_H
#define PROPERTY_H

#include <exotica/Tools/Printable.h>
#include <exotica/Tools/Exception.h>
#include <memory>
#include <map>
#include <vector>
#include <typeinfo>

namespace exotica
{

class PropertyElement : public Printable
{
public:
    PropertyElement();
    PropertyElement(bool isSet, bool isRequired,const std::string type, const std::string name);
    void operator=(PropertyElement& other);
    void operator=(const PropertyElement& other);
    bool isSet() const;
    bool isRequired() const;
    std::string getType() const;
    std::string getName() const;
    virtual void print(std::ostream& os) const;
    virtual void copyValues(PropertyElement& other) = 0;
    virtual void copyValues(const PropertyElement& other) = 0;
protected:
    bool isSet_;
    bool isRequired_;
    std::string name_;
    std::string type_;
};

template<typename T>
class Property : public PropertyElement
{
public:
    // Various constructors
    Property() = default;
    Property(const std::string& type, const std::string& name) : PropertyElement(false, true, type, name) {}
    Property(const std::string& type, const std::string& name, bool isRequired) : PropertyElement(false, isRequired, type, name) {}
    Property(const std::string& type, const std::string& name, bool isRequired, T& value) : PropertyElement(true, isRequired, type, name),value_(value) {}
    Property(const std::string& type, const std::string& name, bool isRequired, const T& value) : PropertyElement(true, isRequired, type, name),value_(value) {}
    Property(Property& obj) : PropertyElement(obj.isSet_,obj.isRequired_, obj.type_, obj.name_), value_(obj.value_){}
    Property(const Property& obj) : PropertyElement(obj.isSet_,obj.isRequired_, obj.type_, obj.name_), value_(obj.value_){}

    virtual void copyValues(PropertyElement& other)
    {
        if(other.isSet()) value_=static_cast<Property<T>&>(other).value_;
    }

    virtual void copyValues(const PropertyElement& other)
    {
        if(other.isSet()) value_=static_cast<const Property<T>&>(other).value_;
    }

    // Construction from unique pointer as a copy constructor inside registerProperty (special case)
    Property(std::unique_ptr<Property<T>> obj) : PropertyElement(obj->isSet_,obj->isRequired_, obj->type_, obj->name_),value_(obj->value_) {}

    // Assign contained value
    // This makes it possible to do:
    // Property<T> prop = T();
    // Just like dealing with T directly.
    void operator=(const T& val)
    {
        value_ = val;
        isSet_ = true;
    }

    void operator=(T& val)
    {
        value_ = val;
        isSet_ = true;
    }

    const T getValue() const {return value_;}

    // Return contained value
    // This makes it possible to do:
    // T val = prop;
    // Just like dealing with T directly.
    operator T() {return value_;}

    virtual void print(std::ostream& os) const
    {
        PropertyElement::print(os);
    }

private:
    T value_;
};

template<typename T>
inline T operator+(T lhs, const Property<T>& rhs)
{
  return lhs + rhs.getValue();
}

inline std::string operator+(const char* lhs, const Property<std::string>& rhs)
{
  return std::string(lhs) + rhs.getValue();
}

template<typename T>
inline T operator-(T lhs, const Property<T>& rhs)
{
  return lhs - rhs.getValue();
}

template<typename T>
inline T operator*(T lhs, const Property<T>& rhs)
{
  return lhs * rhs.getValue();
}

template<typename T>
inline T operator/(T lhs, const Property<T>& rhs)
{
  return lhs / rhs.getValue();
}

// All user property containers must inherit from this.
// Use CMake tool to generate Initializers inheriting from this class
class PropertyContainer : public Printable
{
public:
    virtual void print(std::ostream& os) const
    {
        os << "Container '" << name_ << "'\n";
        for(auto& prop : propertiesOrdered_)
          os << "  " << *prop <<"\n";
    }

    PropertyContainer() : name_("") {}
    PropertyContainer(const std::string& name) : name_(name) {}
    std::string getName() const {return name_;}

    template<typename C> void getProperty(std::string Name, C& ret) const
    {
        ret = static_cast<Property<C>*>(properties_.at(Name))->getValue();
    }


    const std::map<std::string, PropertyElement*>& getProperties() const {return properties_;}
    std::map<std::string, PropertyElement*>& getProperties() {return properties_;}
    const std::vector<PropertyElement*>& getPropertiesOrdered() const {return propertiesOrdered_;}
    std::vector<PropertyElement*>& getPropertiesOrdered() {return propertiesOrdered_;}

protected:
    std::string name_;
    std::map<std::string, PropertyElement*> properties_;
    std::vector<PropertyElement*> propertiesOrdered_;
};

class InstantiableBase
{
public:
    virtual const PropertyContainer& getInitializerTemplate() = 0;
    virtual void InstantiateInternal(const PropertyContainer& init) = 0;
    virtual void InstantiateBase(const PropertyContainer& init) = 0;
};

template<class C>
class Instantiable : public virtual InstantiableBase
{
public:


    virtual void InstantiateInternal(const PropertyContainer& init)
    {
        InstantiateBase(init);
        if(const_cast<PropertyContainer&>(init).getName()==C::getContainerName())
        {
            Instantiate(static_cast<C&>(const_cast<PropertyContainer&>(init)));
        }
        else
        {
            C tmp;
            for(auto& param : tmp.getProperties())
            {
                if(init.getProperties().find(param.first)!= init.getProperties().end())
                {
                    // Copies over typeless PropertyElements using a virtual copyValue method
                    *tmp.getProperties()[param.first] = *init.getProperties().at(param.first);
                }
                else
                {
                    //problem
                    throw_pretty("Combining incompatible initializers!");
                }
            }
            Instantiate(tmp);
        }
    }

    virtual const PropertyContainer& getInitializerTemplate()
    {
        return C();
    }

    virtual void Instantiate(C& init) = 0;
};

}

#endif // PROPERTY_H
