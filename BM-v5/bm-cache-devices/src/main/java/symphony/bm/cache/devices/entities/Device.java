package symphony.bm.cache.devices.entities;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonGetter;
import com.fasterxml.jackson.annotation.JsonIgnore;
import com.fasterxml.jackson.annotation.JsonProperty;
import lombok.Getter;
import lombok.NonNull;
import lombok.RequiredArgsConstructor;
import lombok.Setter;
import org.springframework.data.annotation.PersistenceConstructor;
import org.springframework.data.annotation.Transient;
import symphony.bm.cache.devices.adaptors.Adaptor;
import symphony.bm.cache.devices.entities.deviceproperty.DeviceProperty;

import java.util.List;

public class Device extends Entity {
    @JsonProperty("CID") @NonNull @Getter(onMethod_ = {@JsonProperty("CID")}) private String CID;
    @JsonProperty("RID") @NonNull  @Getter(onMethod_ = {@JsonProperty("RID")}) private String RID;
    @NonNull @Getter private String name;
    @NonNull @Getter private List<DeviceProperty> properties;
    
    @Transient @JsonIgnore @Getter(onMethod_ = {@JsonIgnore}) private Room room = null;
    
    @JsonIgnore private boolean registered = false;

    public Device(@JsonProperty("CID") String CID, @JsonProperty("RID") String RID, @JsonProperty("name") String name,
                  @JsonProperty("properties") List<DeviceProperty> properties) {
        this.CID = CID;
        this.name = name;
        this.RID = RID;
        this.properties = properties;
    }

    public void registerDeviceInAdaptors() throws Exception {
        if (!registered) {
            registered = true;
            for (Adaptor adaptor : adaptors) {
                adaptor.deviceCreated(this);
            }
        }
    }

    public void unregisterDeviceInAdaptors() throws Exception {
        for (Adaptor adaptor : adaptors) {
            adaptor.deviceDeleted(this);
        }
    }

    public boolean hasPropertyIndex(int index) {
        try {
            properties.get(index);
            return true;
        } catch (ArrayIndexOutOfBoundsException e) {
            return false;
        }
    }
    
    public void putProperty(DeviceProperty property) {
        properties.add(property);
    }
    
    public DeviceProperty getProperty(int index) {
        return properties.get(index);
    }
    
    @JsonIgnore
    public Room getFirstAncestorRoom() {
        return room.getFirstAncestorRoom();
    }
    
    @JsonIgnore
    public void setName(String name) throws Exception {
        this.name = name;
        for (Adaptor adaptor : adaptors) {
            adaptor.deviceUpdated(this);
        }
    }
    
    @JsonIgnore
    public void setRoom(Room room) throws Exception {
        if (this.room != null) {
            this.room.transferDevice(CID, room);
        }
        this.room = room;
        this.RID = room.getRID();
        for (Adaptor adaptor : adaptors) {
            adaptor.deviceUpdated(this);
        }
    }
    
    @Override
    protected void setAdaptorsToChildren(List<Adaptor> adaptors) {
        for (DeviceProperty p : properties) {
            p.setAdaptors(adaptors);
        }
    }
    
    @Override
    protected void setSelfToChildren() {
        for (DeviceProperty p : properties) {
            p.setDevice(this);
        }
    }
}
