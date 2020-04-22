package symphony.bm.cache.devices.rest.models;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonProperty;
import lombok.EqualsAndHashCode;
import lombok.NonNull;
import lombok.Value;

@EqualsAndHashCode(callSuper = true)
@Value
public class BasicRegisterRequest extends RegisterRequest {
    String product;
    
    @JsonCreator
    public BasicRegisterRequest(@NonNull @JsonProperty("CID") String CID, @NonNull @JsonProperty("name") String name,
                                @NonNull @JsonProperty("room") String room,
                                @NonNull @JsonProperty("product") String product) {
        super(CID, name, room);
        this.product = product;
    }
}
