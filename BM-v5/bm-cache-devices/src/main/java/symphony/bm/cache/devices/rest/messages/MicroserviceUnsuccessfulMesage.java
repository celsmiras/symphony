package symphony.bm.cache.devices.rest.messages;

public class MicroserviceUnsuccessfulMesage extends MicroserviceMessage {
    private String msg;
    
    public MicroserviceUnsuccessfulMesage(String msg) {
        super(false);
        this.msg = msg;
    }
    
    public String getMsg() {
        return msg;
    }
}
